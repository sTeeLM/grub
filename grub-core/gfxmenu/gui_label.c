/* gui_label.c - GUI component to display a line of text.  */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2008,2009  Free Software Foundation, Inc.
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

#include <grub/mm.h>
#include <grub/misc.h>
#include <grub/gui.h>
#include <grub/font.h>
#include <grub/gui_string_util.h>
#include <grub/i18n.h>
#include <grub/color.h>
#include <grub/env.h>

static const char *align_options[] =
{
  "left",
  "center",
  "right",
  0
};

enum align_mode {
  align_left,
  align_center,
  align_right
};

struct grub_gui_label
{
  struct grub_gui_component comp;

  grub_gui_container_t parent;
  grub_video_rect_t bounds;
  char *id;
  int visible;
  char *text;
  char *template;
  grub_font_t font;
  grub_video_rgba_color_t color;
  int value;
  enum align_mode align;
  int multi_line;
};

typedef struct grub_gui_label *grub_gui_label_t;

static void
label_destroy (void *vself)
{
  grub_gui_label_t self = vself;
  grub_gfxmenu_timeout_unregister ((grub_gui_component_t) self);
  grub_gfxmenu_help_message_unregister((grub_gui_component_t) self);
  grub_free (self->text);
  grub_free (self->template);
  grub_free (self);
}

static const char *
label_get_id (void *vself)
{
  grub_gui_label_t self = vself;
  return self->id;
}

static int
label_is_instance (void *vself __attribute__((unused)), const char *type)
{
  return grub_strcmp (type, "component") == 0;
}

static unsigned int 
label_font_print_sentence(
          const char * text,
          grub_font_t font, 
          grub_video_rgba_color_t color,
          const grub_video_rect_t * bounds,
          enum align_mode align,
          unsigned int * height,
          int multi_line,
          int dry_run);

static void
label_paint (void *vself, const grub_video_rect_t *region)
{
  grub_gui_label_t self = vself;

  if (!self->visible)
    return;

  if (!grub_video_have_common_points (region, &self->bounds))
    return;

  grub_video_rect_t vpsave;
  grub_gui_set_viewport (&self->bounds, &vpsave);

  label_font_print_sentence(
    self->text,
    self->font,
    self->color,
    &self->bounds,
    self->align, 
    NULL,
    self->multi_line,
    0);

  grub_gui_restore_viewport (&vpsave);
}

static void
label_set_parent (void *vself, grub_gui_container_t parent)
{
  grub_gui_label_t self = vself;
  self->parent = parent;
}

static grub_gui_container_t
label_get_parent (void *vself)
{
  grub_gui_label_t self = vself;
  return self->parent;
}

static void
label_set_bounds (void *vself, const grub_video_rect_t *bounds)
{
  grub_gui_label_t self = vself;
  self->bounds = *bounds;
}

static void
label_get_bounds (void *vself, grub_video_rect_t *bounds)
{
  grub_gui_label_t self = vself;
  *bounds = self->bounds;
}

/* Calculate the starting x coordinate.  */
static int
label_get_left_x(
    int length, 
    enum align_mode align,
    const grub_video_rect_t *bounds)
{
  int left_x;
  if (align == align_left)
    left_x = 0;
  else if (align == align_center)
    left_x = (bounds->width - length) / 2;
  else if (align == align_right)
    left_x = (bounds->width - length);
  else
    left_x = -1;   /* Invalid alignment.  */
   if (left_x < 0 || left_x > (int) bounds->width)
    left_x = 0;
  return left_x;
}

/*
  return max width of sentence (with font),
  and total height
*/
static unsigned int 
label_font_print_sentence(
          const char * text,
          grub_font_t font,
          grub_video_rgba_color_t color,
          const grub_video_rect_t * bounds,
          enum align_mode align,
          unsigned int * height,
          int multi_line,
          int dry_run)
{
  char * ptr_head, *ptr;
  char * str = NULL;
  int sentence_len, sentence_cnt, width;
  int left_x = 0, y = 0;

  if(!text)
    return 0;

  if(height)
    *height = 0;

  width = 0;
  sentence_cnt = 0;

  if(!multi_line) 
    {
      width = grub_font_get_string_width (
        font, text);
      y = grub_font_get_ascent (font);
      if(!dry_run)
        {
          left_x = label_get_left_x(width,
            align, bounds);
          grub_font_draw_string (text, font,
            grub_video_map_rgba_color (color),
            left_x, y);
	}
      sentence_cnt = 1;
    }
   else
    {
      if(!(str = grub_strdup(text)))
        return 0;

      ptr_head = str;
      for(ptr = str; ptr < str + grub_strlen(text); ptr ++)
        {
          if(*ptr == '\n')
            {
              *ptr = '\0';
              if(*ptr_head != '\0')
                {
                  // a non empty sentense
                  sentence_len = grub_font_get_string_width(font, ptr_head);
                  y = sentence_cnt * (grub_font_get_ascent (font)
                          + grub_font_get_descent (font)) 
                          + grub_font_get_ascent (font);
                  if(!dry_run)
                    {
                      left_x = label_get_left_x(sentence_len, align, bounds);
                      grub_font_draw_string (ptr_head,
                        font,
                        grub_video_map_rgba_color (color),
                        left_x, y);
                    }
                  width = width > sentence_len ? width : sentence_len;
                }
              else
                {
                  // an empty sentense

                }
              ptr_head = ptr + 1;
              sentence_cnt ++;
            } 
        }
      if(ptr_head < ptr) 
        {
          sentence_len = grub_font_get_string_width(font, ptr_head);
          y = sentence_cnt * (grub_font_get_ascent (font)
                + grub_font_get_descent (font)) 
                + grub_font_get_ascent (font);
          if(!dry_run)
          {
            left_x = label_get_left_x(sentence_len, align, bounds);
            grub_font_draw_string (ptr_head,
              font,
              grub_video_map_rgba_color (color),
              left_x, y);
          }
         width = width > sentence_len ? width : sentence_len;
         sentence_cnt ++;
        }
      grub_free(str);
    }

  if(height)
    *height = y + grub_font_get_descent (font);

  return width;
}


static void
label_get_minimal_size (void *vself, unsigned *width, unsigned *height)
{
  grub_gui_label_t self = vself;

  *width = label_font_print_sentence(
          self->text,
          self->font, 
          self->color,
          &self->bounds,
          self->align,
          height,
          self->multi_line,
          1);
}

#pragma GCC diagnostic ignored "-Wformat-nonliteral"

static void
label_set_state (void *vself, int visible, int start __attribute__ ((unused)),
         int current, int end __attribute__ ((unused)))
{
  grub_gui_label_t self = vself;  
  self->value = -current;
  self->visible = visible;
  grub_free (self->text);
  self->text = grub_xasprintf (self->template ? : "%d", self->value);
}

static grub_err_t
label_set_property (void *vself, const char *name, const char *value)
{
  grub_gui_label_t self = vself;
  if (grub_strcmp (name, "text") == 0)
    {
      grub_free (self->text);
      grub_free (self->template);
      if (! value)
    {
      self->template = NULL;
      self->text = grub_strdup ("");
    }
      else
    {
       if (grub_strcmp (value, "@KEYMAP_LONG@") == 0)
        value = _("Press enter to boot the selected OS, "
           "`e' to edit the commands before booting "
           "or `c' for a command-line. ESC to return previous menu.");
       else if (grub_strcmp (value, "@KEYMAP_MIDDLE@") == 0)
        value = _("Press enter to boot the selected OS, "
           "`e' to edit the commands before booting "
           "or `c' for a command-line.");
       else if (grub_strcmp (value, "@KEYMAP_SHORT@") == 0)
        value = _("enter: boot, `e': options, `c': cmd-line");
       else if (grub_strcmp (value, "@KEYMAP_SCROLL_ENTRY@") == 0)
        value = _("ctrl+l: scroll entry left, ctrl+r: scroll entry right");
       else if (value[0] == '@' && value[1] == '@' && value[2] != '\0')
       {
         value = grub_env_get (&value[2]);
         if (!value)
           value = "";
       }
       /* FIXME: Add more templates here if needed.  */
      self->template = grub_strdup (value);
      self->text = grub_xasprintf (value, self->value);
    }
    }
  else if (grub_strcmp (name, "var") == 0)
    {
      grub_free (self->text);
      grub_free (self->template);
      self->template = NULL;
      const char *str = NULL;
      if (value)
        str = grub_env_get (value);
      if (!str)
        str = "";
      self->template = grub_strdup (str);
      self->text = grub_xasprintf (str, self->value);
    }
  else if (grub_strcmp (name, "font") == 0)
    {
      self->font = grub_font_get (value);
    }
  else if (grub_strcmp (name, "color") == 0)
    {
      grub_video_parse_color (value, &self->color);
    }
  else if (grub_strcmp (name, "align") == 0)
    {
      int i;
      for (i = 0; align_options[i]; i++)
        {
          if (grub_strcmp (align_options[i], value) == 0)
            {
              self->align = i;   /* Set the alignment mode.  */
              break;
            }
        }
    }
  else if (grub_strcmp (name, "visible") == 0)
    {
      self->visible = grub_strcmp (value, "false") != 0;
    }
  else if (grub_strcmp (name, "id") == 0)
    {
      grub_gfxmenu_timeout_unregister ((grub_gui_component_t) self);
      grub_free (self->id);
      if (value)
        self->id = grub_strdup (value);
      else
        self->id = 0;
      if (self->id && grub_strcmp (self->id, GRUB_GFXMENU_TIMEOUT_COMPONENT_ID)
      == 0)
    grub_gfxmenu_timeout_register ((grub_gui_component_t) self,
                       label_set_state);
      if(self->id && grub_strcmp (self->id, GRUB_GFXMENU_HELP_MESSAGE_COMPONENT_ID)
     == 0)
   grub_gfxmenu_help_message_register((grub_gui_component_t) self, label_set_state);
    }
  else if (grub_strcmp (name, "multi_line") == 0)
    {
      self->multi_line = grub_strcmp (value, "false") != 0;
	}
  return GRUB_ERR_NONE;
}

#pragma GCC diagnostic error "-Wformat-nonliteral"

static struct grub_gui_component_ops label_ops =
{
  .destroy = label_destroy,
  .get_id = label_get_id,
  .is_instance = label_is_instance,
  .paint = label_paint,
  .set_parent = label_set_parent,
  .get_parent = label_get_parent,
  .set_bounds = label_set_bounds,
  .get_bounds = label_get_bounds,
  .get_minimal_size = label_get_minimal_size,
  .set_property = label_set_property
};

grub_gui_component_t
grub_gui_label_new (void)
{
  grub_gui_label_t label;
  label = grub_zalloc (sizeof (*label));
  if (! label)
    return 0;
  label->comp.ops = &label_ops;
  label->visible = 1;
  label->text = grub_strdup ("");
  label->font = grub_font_get ("Unknown Regular 16");
  label->color.red = 0;
  label->color.green = 0;
  label->color.blue = 0;
  label->color.alpha = 255;
  label->align = align_left;
  label->multi_line = 0;
  return (grub_gui_component_t) label;
}
