/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2019,2020  Free Software Foundation, Inc.
 *
 *  GRUB is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  GRUB is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GRUB.  If not, see <http://www.gnu.org/licenses/>.
 */

/* 
 * Copyright (C) 2011 VMLite, Inc.
 * 
 * http://www.vmlite.com
 *
 * Authors: Huihong Luo and Bin He
 *
 * This file is part of VMLite VBoot for Linux open source project, as
 * available from http://www.vmlite.com. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * distribution. VBoot Linux Open Source Project is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.  
 *
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#include <grub/dl.h>
#include <grub/misc.h>
#include <grub/file.h>
#include <grub/mm.h>

/* begin of common VHD code, better to put this to a separate file */

GRUB_MOD_LICENSE ("GPLv3+");

/** @def DECLINLINE
 * How to declare a function as inline.
 * @param   type    The return type of the function declaration.
 * @remarks Don't use this macro on C++ methods.
 */
# define DECLINLINE(type) static __inline__ type

/** 1 M (Mega)                 (1 048 576). */
#define _1M                     0x00100000

#define NIL_RTFILE NULL
#define RT_SUCCESS(rc) (rc == GRUB_ERR_NONE)
#define RT_FAILURE(rc) (rc != GRUB_ERR_NONE)

/** @def RT_MIN
 * Finds the minimum value.
 * @returns The lower of the two.
 * @param   Value1      Value 1
 * @param   Value2      Value 2
 */
#define RT_MIN(Value1, Value2)                  ( (Value1) <= (Value2) ? (Value1) : (Value2) )

/** @name VBox HDD container image flags
 * @{
 */
/** No flags. */
#define VD_IMAGE_FLAGS_NONE                     (0)
/** Fixed image. */
#define VD_IMAGE_FLAGS_FIXED                    (0x10000)
/** Diff image. Mutually exclusive with fixed image. */
#define VD_IMAGE_FLAGS_DIFF                     (0x20000)
/** VMDK: Split image into 2GB extents. */
#define VD_VMDK_IMAGE_FLAGS_SPLIT_2G            (0x0001)
/** VMDK: Raw disk image (giving access to a number of host partitions). */
#define VD_VMDK_IMAGE_FLAGS_RAWDISK             (0x0002)
/** VMDK: stream optimized image, read only. */
#define VD_VMDK_IMAGE_FLAGS_STREAM_OPTIMIZED    (0x0004)
/** VMDK: ESX variant, use in addition to other flags. */
#define VD_VMDK_IMAGE_FLAGS_ESX                 (0x0008)
/** VDI: Fill new blocks with zeroes while expanding image file. Only valid
 * for newly created images, never set for opened existing images. */
#define VD_VDI_IMAGE_FLAGS_ZERO_EXPAND          (0x0100)

/** Mask of valid image flags for VMDK. */
#define VD_VMDK_IMAGE_FLAGS_MASK            (   VD_IMAGE_FLAGS_FIXED | VD_IMAGE_FLAGS_DIFF | VD_IMAGE_FLAGS_NONE \
                                             |  VD_VMDK_IMAGE_FLAGS_SPLIT_2G | VD_VMDK_IMAGE_FLAGS_RAWDISK \
                                             | VD_VMDK_IMAGE_FLAGS_STREAM_OPTIMIZED | VD_VMDK_IMAGE_FLAGS_ESX)

/** Mask of valid image flags for VDI. */
#define VD_VDI_IMAGE_FLAGS_MASK             (VD_IMAGE_FLAGS_FIXED | VD_IMAGE_FLAGS_DIFF | VD_IMAGE_FLAGS_NONE | VD_VDI_IMAGE_FLAGS_ZERO_EXPAND)

/** Mask of all valid image flags for all formats. */
#define VD_IMAGE_FLAGS_MASK                 (VD_VMDK_IMAGE_FLAGS_MASK | VD_VDI_IMAGE_FLAGS_MASK)

/** Default image flags. */
#define VD_IMAGE_FLAGS_DEFAULT              (VD_IMAGE_FLAGS_NONE)
/** @} */

#define VHD_RELATIVE_MAX_PATH 512
#define VHD_ABSOLUTE_MAX_PATH 512

#define VHD_SECTOR_SIZE 512
#define VHD_SECTOR_SIZE_SHIFT 9
#define VHD_BLOCK_SIZE  (2 * _1M)

// each entry requires 4 bytes in memory, 1024 entries will use 4K memory
#define VHD_ALLOCATION_TABLE_ENTRIES 1024

typedef char bool;

/* This is common to all VHD disk types and is located at the end of the image */
#pragma pack(1)
typedef struct VHDFooter
{
    char     Cookie[8];
    grub_uint32_t Features;
    grub_uint32_t Version;
    grub_uint64_t DataOffset;
    grub_uint32_t TimeStamp;
    grub_uint8_t  CreatorApp[4];
    grub_uint32_t CreatorVer;
    grub_uint32_t CreatorOS;
    grub_uint64_t OrigSize;
    grub_uint64_t CurSize;
    grub_uint16_t DiskGeometryCylinder;
    grub_uint8_t  DiskGeometryHeads;
    grub_uint8_t  DiskGeometrySectors;
    grub_uint32_t DiskType;
    grub_uint32_t Checksum;
    char     UniqueID[16];
    grub_uint8_t  SavedState;
    grub_uint8_t  Reserved[427];
} VHDFooter;
#pragma pack()

#define VHD_FOOTER_COOKIE "conectix"
#define VHD_FOOTER_COOKIE_SIZE 8

#define VHD_FOOTER_FEATURES_NOT_ENABLED   0
#define VHD_FOOTER_FEATURES_TEMPORARY     1
#define VHD_FOOTER_FEATURES_RESERVED      2

#define VHD_FOOTER_FILE_FORMAT_VERSION    0x00010000
//#define VHD_FOOTER_DATA_OFFSET_FIXED      UINT64_C(0xffffffffffffffff)
#define VHD_FOOTER_DISK_TYPE_FIXED        2
#define VHD_FOOTER_DISK_TYPE_DYNAMIC      3
#define VHD_FOOTER_DISK_TYPE_DIFFERENCING 4

#define VHD_MAX_LOCATOR_ENTRIES           8
#define VHD_PLATFORM_CODE_NONE            0
#define VHD_PLATFORM_CODE_WI2R            0x57693272
#define VHD_PLATFORM_CODE_WI2K            0x5769326B
#define VHD_PLATFORM_CODE_W2RU            0x57327275
#define VHD_PLATFORM_CODE_W2KU            0x57326B75
#define VHD_PLATFORM_CODE_MAC             0x4D163220
#define VHD_PLATFORM_CODE_MACX            0x4D163258

/* Header for expanding disk images. */
#pragma pack(1)
typedef struct VHDParentLocatorEntry
{
    grub_uint32_t u32Code;
    grub_uint32_t u32DataSpace;
    grub_uint32_t u32DataLength;
    grub_uint32_t u32Reserved;
    grub_uint64_t u64DataOffset;
} VHDPLE, *PVHDPLE;

typedef struct VHDDynamicDiskHeader
{
    char     Cookie[8];
    grub_uint64_t DataOffset;
    grub_uint64_t TableOffset;
    grub_uint32_t HeaderVersion;
    grub_uint32_t MaxTableEntries;
    grub_uint32_t BlockSize;
    grub_uint32_t Checksum;
    grub_uint8_t  ParentUuid[16];
    grub_uint32_t ParentTimeStamp;
    grub_uint32_t Reserved0;
    grub_uint8_t  ParentUnicodeName[512];
    VHDPLE   ParentLocatorEntry[VHD_MAX_LOCATOR_ENTRIES];
    grub_uint8_t  Reserved1[256];
} VHDDynamicDiskHeader;
#pragma pack()

#define VHD_DYNAMIC_DISK_HEADER_COOKIE "cxsparse"
#define VHD_DYNAMIC_DISK_HEADER_COOKIE_SIZE 8
#define VHD_DYNAMIC_DISK_HEADER_VERSION 0x00010000

/**
 * UUID data type.
 *
 * @note IPRT defines that the first three integers in the @c Gen struct
 * interpretation are in little endian representation. This is different to
 * many other UUID implementation, and requires conversion if you need to
 * achieve consistent results.
 */
typedef union RTUUID
{
    /** 8-bit view. */
    grub_uint8_t     au8[16];
    /** 16-bit view. */
    grub_uint16_t    au16[8];
    /** 32-bit view. */
    grub_uint32_t    au32[4];
    /** 64-bit view. */
    grub_uint64_t    au64[2];
    /** The way the UUID is declared by the DCE specification. */
    struct
    {
        grub_uint32_t    u32TimeLow;
        grub_uint16_t    u16TimeMid;
        grub_uint16_t    u16TimeHiAndVersion;
        grub_uint8_t     u8ClockSeqHiAndReserved;
        grub_uint8_t     u8ClockSeqLow;
        grub_uint8_t     au8Node[6];
    } Gen;
} RTUUID;
/** Pointer to UUID data. */
typedef RTUUID *PRTUUID;
/** Pointer to readonly UUID data. */
typedef const RTUUID *PCRTUUID;

/**
 * Media geometry structure.
 */
typedef struct PDMMEDIAGEOMETRY
{
    /** Number of cylinders. */
    grub_uint32_t    cCylinders;
    /** Number of heads. */
    grub_uint32_t    cHeads;
    /** Number of sectors. */
    grub_uint32_t    cSectors;
} PDMMEDIAGEOMETRY;

/** Pointer to media geometry structure. */
typedef PDMMEDIAGEOMETRY *PPDMMEDIAGEOMETRY;
/** Pointer to constant media geometry structure. */
typedef const PDMMEDIAGEOMETRY *PCPDMMEDIAGEOMETRY;

/**
 * Complete VHD image data structure.
 */
typedef struct VHDIMAGE
{
    /** Descriptor file if applicable. */
    grub_file_t          File;

    /** Open flags passed by VBoxHD layer. not used in BIOS */
    grub_uint32_t        uOpenFlags;
    /** Image flags defined during creation or determined during open. */
    grub_uint32_t        uImageFlags;

    /** Total size of the image. */
    grub_uint64_t        cbSize;
    /** Original size of the image. */
    grub_uint64_t        cbOrigSize;
    /** Physical geometry of this image. */
    PDMMEDIAGEOMETRY    PCHSGeometry;
    /** Logical geometry of this image. */
    PDMMEDIAGEOMETRY    LCHSGeometry;
    /** Image UUID. */
    RTUUID          ImageUuid;
    /** Parent image UUID. */
    RTUUID          ParentUuid;
    /** Parent's time stamp at the time of image creation. */
    grub_uint32_t        u32ParentTimeStamp;
    /** Relative path to the parent image. */
    char            *pszParentFilename;
    /** File size on the host disk (including all headers). */
    grub_uint64_t        FileSize;

    /** The Block Allocation Table, due to small memory in real mode, we can't real all entries */
    grub_uint32_t        *pBlockAllocationTable;
    /** Total Number of entries in the table. */
    grub_uint32_t        cBlockAllocationTableEntries;

    /** the start block entry in the table, loaded from vhd file, this 0 or 512 multiples. */
    grub_uint32_t         cBlockAllocationTableStartEntry;

    /** Number of entries in the table, loaded from vhd file, 
        we use 512 entries, so the table takes 2K in RAM, covers 1G disk for 2M block */
    grub_uint32_t         cBlockAllocationTableEntriesInCache;

    /** Size of one data block. */
    grub_uint32_t        cbDataBlock;
    /** Sectors per data block. */
    grub_uint32_t        cSectorsPerDataBlock;
    grub_uint32_t        cSectorsPerDataBlockShift;
    /** Length of the sector bitmap in bytes. */
    grub_uint32_t        cbDataBlockBitmap;
    /** A copy of the disk footer. */
    VHDFooter       vhdFooterCopy;
    /** Current end offset of the file (without the disk footer). */
    grub_uint64_t        uCurrentEndOfFile;
    /** Start offset of data blocks. */
    grub_uint64_t        uDataBlockStart;
    /** Size of the data block bitmap in sectors. */
    grub_uint32_t        cDataBlockBitmapSectors;
    /** Start of the block allocation table. */
    grub_uint64_t        uBlockAllocationTableOffset;
    /** Buffer to hold block's bitmap for bit search operations. */
    grub_uint8_t         *pu8Bitmap;
    /** Offset to the next data structure (dynamic disk header). */
    grub_uint64_t        u64DataOffset;
    /** Flag to force dynamic disk header update. */
    bool            fDynHdrNeedsUpdate;
} VHDIMAGE, *PVHDIMAGE;

/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/

static int vhdLoadDynamicDisk(PVHDIMAGE pImage, grub_uint64_t uDynamicDiskHeaderOffset);

/* endians */

#ifdef GRUB_CPU_WORDS_BIGENDIAN

#define RT_BE2H_U16(x) ((grub_uint16_t) (x)
#define RT_BE2H_U32(x) ((grub_uint32_t) (x)
#define RT_BE2H_U64(x) ((grub_uint64_t) (x))

#else

/** @def RT_BE2H_U16
 * Converts an uint16_t value from big endian to host byte order. */
#define RT_BE2H_U16(x) (grub_swap_bytes16 (x))

/** @def RT_BE2H_U32
 * Converts an uint32_t value from big endian to host byte order. */
#define RT_BE2H_U32(x) (grub_swap_bytes32 (x))

/** @def RT_BE2H_U64
 * Converts an uint64_t value from big endian to host byte order. */
#define RT_BE2H_U64(x) (grub_swap_bytes64 (x))

#endif

/*
 *  Determine whether some value is a power of two, where zero is
 * *not* considered a power of two.
 */
DECLINLINE(bool) is_power_of_2(grub_uint32_t n)
{
    return (n != 0 && ((n & (n - 1)) == 0));
}

DECLINLINE(grub_uint32_t) _log2(grub_uint32_t x)
{
    int targetlevel = 0; 
    while (x >>= 1) ++targetlevel; 

    return targetlevel;
}

/**
 * Tests if a bit in a bitmap is set.
 *
 * @returns true if the bit is set.
 * @returns false if the bit is clear.
 *
 * @param   pvBitmap    Pointer to the bitmap.
 * @param   iBit        The bit to test.
 *
 * @remarks The 32-bit aligning of pvBitmap is not a strict requirement.
 *          However, doing so will yield better performance as well as avoiding
 *          traps accessing the last bits in the bitmap.
 */
DECLINLINE(bool) ASMBitTest(const volatile void *pvBitmap, grub_uint32_t iBit)
{
   return (((((grub_uint32_t *)pvBitmap)[(iBit) / 32]) >> ((iBit) % 32)) & 0x1);
}

static grub_uint64_t vhdGetSize(void *pBackendData)
{
    PVHDIMAGE pImage = (PVHDIMAGE)pBackendData;

    if (pImage)
        return pImage->cbSize;
    else
        return 0;
}

static int vhdFileGetSize(PVHDIMAGE pImage, grub_uint64_t *pcbSize)
{
    int rc = GRUB_ERR_NONE;

    *pcbSize = pImage->File->size;

    return rc;
}

static int vhdFileReadSync(PVHDIMAGE pImage, grub_uint64_t off, void *pvBuf, grub_size_t cbRead, grub_ssize_t *pcbRead)
{
    int rc = GRUB_ERR_NONE;

    grub_file_seek(pImage->File, off);

    grub_ssize_t bytesRead = grub_file_read(pImage->File, pvBuf, cbRead);

    if (pcbRead)
    {
        *pcbRead = bytesRead;
    }

    return rc;
}

static int vhdOpenImage(PVHDIMAGE pImage)
{
    grub_uint64_t FileSize;
    VHDFooter vhdFooter;

    /*
     * Open the image.
     */
    int rc = 0;

    rc = vhdFileGetSize(pImage, &FileSize);
    pImage->FileSize = FileSize;
    pImage->uCurrentEndOfFile = FileSize - sizeof(VHDFooter);

    // often the time, grub2 has problems reading last sector, so we try the copy of the footer, huihong Luo
    rc = vhdFileReadSync(pImage, 0, &vhdFooter, sizeof(VHDFooter), 0);
    //grub_printf("reading vhd footer from front.., Cookie=%s, rc=%d, sizeof(VHDFooter)=%d\n", vhdFooter.Cookie, rc, sizeof(VHDFooter));
    if (grub_memcmp(vhdFooter.Cookie, VHD_FOOTER_COOKIE, VHD_FOOTER_COOKIE_SIZE) != 0)
    {
        rc = vhdFileReadSync(pImage, pImage->uCurrentEndOfFile, &vhdFooter, sizeof(VHDFooter), 0);
        //grub_printf("reading vhd footer from end.., Cookie=%s, rc=%d\n", vhdFooter.Cookie, rc);
        if (grub_memcmp(vhdFooter.Cookie, VHD_FOOTER_COOKIE, VHD_FOOTER_COOKIE_SIZE) != 0)
        {
            // often the time, grub2 has problems reading last sector, so we try the copy of the footer, huihong Luo
            return GRUB_VD_VHD_INVALID_HEADER;
        }
    }

    switch (RT_BE2H_U32(vhdFooter.DiskType))
    {
        case VHD_FOOTER_DISK_TYPE_FIXED:
            {
                pImage->uImageFlags |= VD_IMAGE_FLAGS_FIXED;
            }
            break;
        case VHD_FOOTER_DISK_TYPE_DYNAMIC:
            {
                pImage->uImageFlags &= ~VD_IMAGE_FLAGS_FIXED;
            }
            break;
        case VHD_FOOTER_DISK_TYPE_DIFFERENCING:
            {
                pImage->uImageFlags |= VD_IMAGE_FLAGS_DIFF;
                pImage->uImageFlags &= ~VD_IMAGE_FLAGS_FIXED;
            }
            break;
        default:
            return GRUB_ERR_NOT_IMPLEMENTED_YET;
    }

    pImage->cbSize       = RT_BE2H_U64(vhdFooter.CurSize);
    pImage->LCHSGeometry.cCylinders   = 0;
    pImage->LCHSGeometry.cHeads       = 0;
    pImage->LCHSGeometry.cSectors     = 0;
    pImage->PCHSGeometry.cCylinders   = RT_BE2H_U16(vhdFooter.DiskGeometryCylinder);
    pImage->PCHSGeometry.cHeads       = vhdFooter.DiskGeometryHeads;
    pImage->PCHSGeometry.cSectors     = vhdFooter.DiskGeometrySectors;

    /*
     * Copy of the disk footer.
     * If we allocate new blocks in differencing disks on write access
     * the footer is overwritten. We need to write it at the end of the file.
     */
    grub_memcpy(&pImage->vhdFooterCopy, &vhdFooter, sizeof(VHDFooter));

    /*
     * Is there a better way?
     */
    grub_memcpy(&pImage->ImageUuid, &vhdFooter.UniqueID, 16);

    pImage->u64DataOffset = RT_BE2H_U64(vhdFooter.DataOffset);

    if (!(pImage->uImageFlags & VD_IMAGE_FLAGS_FIXED))
        rc = vhdLoadDynamicDisk(pImage, pImage->u64DataOffset);

    return rc;
}

/**
 * Internal: Allocates the block bitmap rounding up to the next 32bit or 64bit boundary.
 *           Can be freed with RTMemFree. The memory is zeroed.
 */
DECLINLINE(grub_uint8_t *) vhdBlockBitmapAllocate(PVHDIMAGE pImage)
{
    return (grub_uint8_t *)grub_zalloc(pImage->cbDataBlockBitmap + 8);
}

//static grub_uint32_t g_block_table_cache[VHD_ALLOCATION_TABLE_ENTRIES];

// read the allocation table starting from the specified block start entry
static int vhdLoadAllocationTable(PVHDIMAGE pImage, grub_uint32_t cBlockAllocationTableEntry)
{
    int rc = GRUB_ERR_NONE;
    grub_uint32_t numEntries;
    grub_uint32_t *pBlockAllocationTable;
    unsigned i = 0;

    numEntries = RT_MIN(pImage->cBlockAllocationTableEntries, VHD_ALLOCATION_TABLE_ENTRIES);
    //grub_printf("vhdLoadAllocationTable(numEntries=%d, cBlockAllocationTableEntry=%d)\n", numEntries, cBlockAllocationTableEntry);

    pBlockAllocationTable = (grub_uint32_t *)grub_zalloc(numEntries * sizeof(grub_uint32_t));
    if (!pBlockAllocationTable)
        return GRUB_ERR_OUT_OF_MEMORY;

    //pBlockAllocationTable = g_block_table_cache;
    pImage->cBlockAllocationTableEntriesInCache = numEntries;

    /*
     * Read the table.
     */
    cBlockAllocationTableEntry -= (cBlockAllocationTableEntry % 512);

    rc = vhdFileReadSync(pImage, pImage->uBlockAllocationTableOffset + cBlockAllocationTableEntry * sizeof(grub_uint32_t), 
            pBlockAllocationTable, pImage->cBlockAllocationTableEntriesInCache * sizeof(grub_uint32_t), 0);

    /*
     * Because the offset entries inside the allocation table are stored big endian
     * we need to convert them into host endian.
     */
    if (pImage->pBlockAllocationTable == NULL)
    {
        pImage->pBlockAllocationTable = (grub_uint32_t *)grub_zalloc(pImage->cBlockAllocationTableEntriesInCache * sizeof(grub_uint32_t));
        if (!pImage->pBlockAllocationTable)
            return GRUB_ERR_OUT_OF_MEMORY;
    }

    for (i = 0; i < pImage->cBlockAllocationTableEntriesInCache; i++)
        pImage->pBlockAllocationTable[i] = RT_BE2H_U32(pBlockAllocationTable[i]);

    grub_free(pBlockAllocationTable);

    pImage->cBlockAllocationTableStartEntry = cBlockAllocationTableEntry;

    return rc;
}

static int vhdLoadDynamicDisk(PVHDIMAGE pImage, grub_uint64_t uDynamicDiskHeaderOffset)
{
    VHDDynamicDiskHeader vhdDynamicDiskHeader;
    int rc = GRUB_ERR_NONE;
    grub_uint64_t uBlockAllocationTableOffset;

    //grub_printf("vhdLoadDynamicDisk()\n");

    /*
     * Read the dynamic disk header.
     */
    rc = vhdFileReadSync(pImage, uDynamicDiskHeaderOffset, &vhdDynamicDiskHeader, sizeof(VHDDynamicDiskHeader), 0);
    if (grub_memcmp(vhdDynamicDiskHeader.Cookie, VHD_DYNAMIC_DISK_HEADER_COOKIE, VHD_DYNAMIC_DISK_HEADER_COOKIE_SIZE) != 0)
        return GRUB_ERR_BAD_ARGUMENT;

    pImage->cbDataBlock = RT_BE2H_U32(vhdDynamicDiskHeader.BlockSize);
    //LogFlowFunc(("BlockSize=%u\n", pImage->cbDataBlock));
    pImage->cBlockAllocationTableEntries = RT_BE2H_U32(vhdDynamicDiskHeader.MaxTableEntries);
    //grub_printf("MaxTableEntries=%d\n", pImage->cBlockAllocationTableEntries);
    //AssertMsg(!(pImage->cbDataBlock % VHD_SECTOR_SIZE), ("%s: Data block size is not a multiple of %!\n", __FUNCTION__, VHD_SECTOR_SIZE));

    pImage->cSectorsPerDataBlock = pImage->cbDataBlock / VHD_SECTOR_SIZE;
    //LogFlowFunc(("SectorsPerDataBlock=%u\n", pImage->cSectorsPerDataBlock));

    /*
    Block Size
    A block is a unit of expansion for dynamic and differencing hard disks. 
    It is stored in bytes. This size does not include the size of the block bitmap. 
    It is only the size of the data section of the block. The sectors per block must always be a power of two. 
    The default value is 0x00200000 (indicating a block size of 2 MB).
    */
    pImage->cSectorsPerDataBlockShift = _log2(pImage->cSectorsPerDataBlock); 

    /*
     * Every block starts with a bitmap indicating which sectors are valid and which are not.
     * We store the size of it to be able to calculate the real offset.
     */
    pImage->cbDataBlockBitmap = pImage->cSectorsPerDataBlock / 8;
    pImage->cDataBlockBitmapSectors = pImage->cbDataBlockBitmap / VHD_SECTOR_SIZE;
    //grub_printf("cbDataBlockBitmap=%d\n", pImage->cbDataBlockBitmap);

    pImage->pu8Bitmap = vhdBlockBitmapAllocate(pImage);
    if (!pImage->pu8Bitmap)
        return GRUB_ERR_OUT_OF_MEMORY;

    /*
     * Read the table.
     */
    uBlockAllocationTableOffset = RT_BE2H_U64(vhdDynamicDiskHeader.TableOffset);
    //LogFlowFunc(("uBlockAllocationTableOffset=%llu\n", uBlockAllocationTableOffset));
    pImage->uBlockAllocationTableOffset = uBlockAllocationTableOffset;

    pImage->uDataBlockStart = uBlockAllocationTableOffset + pImage->cBlockAllocationTableEntries * sizeof(grub_uint32_t);
    //LogFlowFunc(("uDataBlockStart=%llu\n", pImage->uDataBlockStart));

    rc = vhdLoadAllocationTable(pImage, 0);

    if (pImage->uImageFlags & VD_IMAGE_FLAGS_DIFF)
        grub_memcpy(pImage->ParentUuid.au8, vhdDynamicDiskHeader.ParentUuid, sizeof(pImage->ParentUuid));

    return rc;
}

/**
 * Internal: Checks if a sector in the block bitmap is set
 */
DECLINLINE(bool) vhdBlockBitmapSectorContainsData(PVHDIMAGE pImage, grub_uint32_t cBlockBitmapEntry)
{
    grub_uint32_t iBitmap = (cBlockBitmapEntry / 8); /* Byte in the block bitmap. */

    /*
     * The index of the bit in the byte of the data block bitmap.
     * The most signifcant bit stands for a lower sector number.
     */
    grub_uint8_t  iBitInByte = (8-1) - (cBlockBitmapEntry % 8);
    grub_uint8_t *puBitmap = pImage->pu8Bitmap + iBitmap;

    //AssertMsg(puBitmap < (pImage->pu8Bitmap + pImage->cbDataBlockBitmap),
    //            ("VHD: Current bitmap position exceeds maximum size of the bitmap\n"));

    return ASMBitTest(puBitmap, iBitInByte);
}

static int vhdRead(void *pBackendData, grub_uint64_t uOffset, void *pvBuf, grub_size_t cbRead, grub_size_t *pcbActuallyRead)
{
    PVHDIMAGE pImage = (PVHDIMAGE)pBackendData;
    int rc = GRUB_ERR_NONE;

    //LogFlowFunc(("pBackendData=%p uOffset=%#llx pvBuf=%p cbRead=%u pcbActuallyRead=%p\n", pBackendData, uOffset, pvBuf, cbRead, pcbActuallyRead));

    if (uOffset + cbRead > pImage->cbSize)
        return GRUB_ERR_BAD_ARGUMENT;

    /*
     * If we have a dynamic disk image, we need to find the data block and sector to read.
     */
    if (pImage->pBlockAllocationTable)
    {
        /*
         * Get the data block first.
         */
        grub_uint32_t cBlockAllocationTableEntry = uOffset >> (VHD_SECTOR_SIZE_SHIFT + pImage->cSectorsPerDataBlockShift);
        //grub_uint32_t cBATEntryIndex = (uOffset >> VHD_SECTOR_SIZE_SHIFT) % pImage->cSectorsPerDataBlock;
        grub_uint64_t cBATEntryIndex;
        grub_divmod64(uOffset >> VHD_SECTOR_SIZE_SHIFT, pImage->cSectorsPerDataBlock, &cBATEntryIndex);

        grub_uint32_t idx;
        grub_uint64_t uVhdOffset;

        /* load the corresponding portion of the allocation table if not in cache */
        if ((cBlockAllocationTableEntry < pImage->cBlockAllocationTableStartEntry) ||
            (cBlockAllocationTableEntry >= pImage->cBlockAllocationTableStartEntry + pImage->cBlockAllocationTableEntriesInCache))
        {
            rc = vhdLoadAllocationTable(pImage, cBlockAllocationTableEntry);
        }

        // index to the allocation table in cache
        idx = cBlockAllocationTableEntry - pImage->cBlockAllocationTableStartEntry;

        //LogFlowFunc(("cBlockAllocationTableEntry=%u cBatEntryIndex=%u\n", cBlockAllocationTableEntry, cBATEntryIndex));
        //LogFlowFunc(("BlockAllocationEntry=%u\n", pImage->pBlockAllocationTable[idx]));

        /*
         * If the block is not allocated the content of the entry is ~0
         */
        if (pImage->pBlockAllocationTable[idx] == ~0U)
        {
            /* Return block size as read. */
            *pcbActuallyRead = RT_MIN(cbRead, pImage->cSectorsPerDataBlock * VHD_SECTOR_SIZE);
            return GRUB_VD_BLOCK_FREE;
        }

        // use the following when compile with gcc
#if defined(__GNUC__)
        uVhdOffset = ((grub_uint64_t)pImage->pBlockAllocationTable[idx] + pImage->cDataBlockBitmapSectors + cBATEntryIndex) << VHD_SECTOR_SIZE_SHIFT;
#else
        uVhdOffset.LowPart = pImage->pBlockAllocationTable[idx];
        uVhdOffset.HighPart = 0;
        uVhdOffset = (uVhdOffset + pImage->cDataBlockBitmapSectors + cBATEntryIndex) << VHD_SECTOR_SIZE_SHIFT;
#endif

        //LogFlowFunc(("uVhdOffset=%llu cbRead=%u\n", uVhdOffset, cbRead));

        /*
         * Clip read range to remain in this data block.
         */
        cbRead = RT_MIN(cbRead, (pImage->cbDataBlock - (cBATEntryIndex * VHD_SECTOR_SIZE)));

        grub_uint64_t uVhdBitmapOffset;

#if defined(__GNUC__)
        uVhdBitmapOffset = ((grub_uint64_t)pImage->pBlockAllocationTable[idx]) * VHD_SECTOR_SIZE;
#else
        uVhdBitmapOffset.LowPart = pImage->pBlockAllocationTable[idx];
        uVhdBitmapOffset.HighPart = 0;
        uVhdBitmapOffset = uVhdBitmapOffset << VHD_SECTOR_SIZE_SHIFT;
#endif

        /* Read in the block's bitmap. */
        rc = vhdFileReadSync(pImage,
                             uVhdBitmapOffset,
                             pImage->pu8Bitmap, pImage->cbDataBlockBitmap, 0);
        if (RT_SUCCESS(rc))
        {
            grub_uint32_t cSectors = 0;

            if (vhdBlockBitmapSectorContainsData(pImage, cBATEntryIndex))
            {
                cBATEntryIndex++;
                cSectors = 1;

                /*
                 * The first sector being read is marked dirty, read as much as we
                 * can from child. Note that only sectors that are marked dirty
                 * must be read from child.
                 */
                while (   (cSectors < (cbRead / VHD_SECTOR_SIZE))
                       && vhdBlockBitmapSectorContainsData(pImage, cBATEntryIndex))
                {
                    cBATEntryIndex++;
                    cSectors++;
                }

                cbRead = cSectors * VHD_SECTOR_SIZE;

                //LogFlowFunc(("uVhdOffset=%llu cbRead=%u\n", uVhdOffset, cbRead));
                rc = vhdFileReadSync(pImage, uVhdOffset, pvBuf, cbRead, 0);
            }
            else
            {
                /*
                 * The first sector being read is marked clean, so we should read from
                 * our parent instead, but only as much as there are the following
                 * clean sectors, because the block may still contain dirty sectors
                 * further on. We just need to compute the number of clean sectors
                 * and pass it to our caller along with the notification that they
                 * should be read from the parent.
                 */
                cBATEntryIndex++;
                cSectors = 1;

                while (   (cSectors < (cbRead / VHD_SECTOR_SIZE))
                       && !vhdBlockBitmapSectorContainsData(pImage, cBATEntryIndex))
                {
                    cBATEntryIndex++;
                    cSectors++;
                }

                cbRead = cSectors * VHD_SECTOR_SIZE;
                //Log(("%s: Sectors free: uVhdOffset=%llu cbRead=%u\n", __FUNCTION__, uVhdOffset, cbRead));
                rc = GRUB_VD_BLOCK_FREE;
            }
        }
        else
        {
            //AssertMsgFailed(("Reading block bitmap failed rc=%Rrc\n", rc));
        }
    }
    else
    {
        rc = vhdFileReadSync(pImage, uOffset, pvBuf, cbRead, 0);
    }

    if (pcbActuallyRead)
        *pcbActuallyRead = cbRead;

    //Log2(("vhdRead: off=%#llx pvBuf=%p cbRead=%d\n"
    //        "%.*Rhxd\n",
     //       uOffset, pvBuf, cbRead, cbRead, pvBuf));

    return rc;
}

static void vhdFreeImageMemory(PVHDIMAGE pImage)
{
    if (pImage->pszParentFilename)
    {
        grub_free(pImage->pszParentFilename);
        pImage->pszParentFilename = NULL;
    }
    if (pImage->pBlockAllocationTable)
    {
        grub_free(pImage->pBlockAllocationTable);
        pImage->pBlockAllocationTable = NULL;
    }
    if (pImage->pu8Bitmap)
    {
        grub_free(pImage->pu8Bitmap);
        pImage->pu8Bitmap = NULL;
    }
    grub_free(pImage);
}

static int vhdFreeImage(PVHDIMAGE pImage)
{
    int rc = GRUB_ERR_NONE;

    /* Freeing a never allocated image (e.g. because the open failed) is
     * not signalled as an error. After all nothing bad happens. */
    if (pImage)
    {
        //vhdFlush(pImage);
        vhdFreeImageMemory(pImage);
    }

    return rc;
}

static int vhdClose(void *pBackendData)
{
    PVHDIMAGE pImage = (PVHDIMAGE)pBackendData;
    int rc = GRUB_ERR_NONE;

    /* Freeing a never allocated image (e.g. because the open failed) is
     * not signalled as an error. After all nothing bad happens. */
    if (pImage)
    {
        rc = vhdFreeImage(pImage);
    }

    return rc;
}

static int vhdOpen(grub_file_t file, void **ppvBackendData)
{
    int rc = GRUB_ERR_NONE;
    PVHDIMAGE pImage;

    pImage = (PVHDIMAGE)grub_zalloc(sizeof(VHDIMAGE));
    if (!pImage)
    {
        rc = GRUB_ERR_OUT_OF_MEMORY;
        return rc;
    }

    if (file)
      pImage->File = file;
    else
      pImage->File = NIL_RTFILE;

    rc = vhdOpenImage(pImage);
    if (RT_SUCCESS(rc))
        *ppvBackendData = pImage;
    return rc;
}
/* end of common VHD code */

struct grub_vhdio
{
  grub_file_t file;
  void *vhd_file;
};
typedef struct grub_vhdio *grub_vhdio_t;

static struct grub_fs grub_vhdio_fs;

static grub_file_t
grub_vhdio_open (grub_file_t io, enum grub_file_type type)
{
  grub_file_t file;
  grub_vhdio_t vhdio;

  if (type & GRUB_FILE_TYPE_NO_DECOMPRESS)
    return io;

  file = (grub_file_t) grub_zalloc (sizeof (*file));
  if (!file)
    return 0;

  vhdio = grub_zalloc (sizeof (*vhdio));
  if (!vhdio)
  {
    grub_free (file);
    return 0;
  }

  vhdio->file = io;

  file->device = io->device;
  file->data = vhdio;
  file->fs = &grub_vhdio_fs;
  file->size = GRUB_FILE_SIZE_UNKNOWN;
  file->not_easily_seekable = io->not_easily_seekable;

  if (grub_file_tell (vhdio->file) != 0)
    grub_file_seek (vhdio->file, 0);

  if (vhdOpen (io, &vhdio->vhd_file))
  {
    grub_errno = GRUB_ERR_NONE;
    vhdClose (vhdio->vhd_file);
    grub_file_seek (io, 0);
    grub_free (vhdio);
    grub_free (file);
    return io;
  }

  file->size = vhdGetSize (vhdio->vhd_file);

  return file;
}

static grub_ssize_t
grub_vhdio_read (grub_file_t file, char *buf, grub_size_t len)
{
  grub_vhdio_t vhdio = file->data;
  void *vhd_file = vhdio->vhd_file;
  grub_size_t actuallyRead;
  int rc;

  actuallyRead = 0;
  rc = vhdRead (vhd_file, grub_file_tell (file), buf, len, &actuallyRead);
  if (rc != GRUB_ERR_NONE)
    return -1;

  return (grub_ssize_t) actuallyRead;
}

static grub_err_t
grub_vhdio_close (grub_file_t file)
{
  grub_vhdio_t vhdio = file->data;
  void *vhd_file = vhdio->vhd_file;

  vhdClose (vhd_file);
  grub_file_close (vhdio->file);
  grub_free (vhdio);
  file->device = 0;
  file->name = 0;
  return grub_errno;
}

static struct grub_fs grub_vhdio_fs = {
  .name = "vhdio",
  .fs_dir = 0,
  .fs_open = 0,
  .fs_read = grub_vhdio_read,
  .fs_close = grub_vhdio_close,
  .fs_label = 0,
  .next = 0
};

GRUB_MOD_INIT(vhd)
{
  grub_file_filter_register (GRUB_FILE_FILTER_VHDIO, grub_vhdio_open);
}

GRUB_MOD_FINI(vhd)
{
  grub_file_filter_unregister (GRUB_FILE_FILTER_VHDIO);
}
