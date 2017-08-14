/*
 * Copyright © 2011  Google, Inc.
 *
 *  This is part of HarfBuzz, a text shaping library.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN
 * IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * THE COPYRIGHT HOLDER SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE COPYRIGHT HOLDER HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * Google Author(s): Behdad Esfahbod
 */

#ifndef OPTIONS_HH
#define OPTIONS_HH


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <locale.h>
#include <errno.h>
#include <fcntl.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h> /* for isatty() */
#endif
#if defined(_WIN32) || defined(__CYGWIN__)
#include <io.h> /* for setmode() under Windows */
#endif

#include <hb.h>
#ifdef HAVE_OT
#include <hb-ot.h>
#endif
#include <glib.h>
#include <glib/gprintf.h>

#if !GLIB_CHECK_VERSION (2, 22, 0)
# define g_mapped_file_unref g_mapped_file_free
#endif


/* A few macros copied from hb-private.hh. */

#if __GNUC__ >= 4
#define HB_UNUSED	__attribute__((unused))
#else
#define HB_UNUSED
#endif

#undef MIN
template <typename Type> static inline Type MIN (const Type &a, const Type &b) { return a < b ? a : b; }

#undef MAX
template <typename Type> static inline Type MAX (const Type &a, const Type &b) { return a > b ? a : b; }

#undef  ARRAY_LENGTH
template <typename Type, unsigned int n>
static inline unsigned int ARRAY_LENGTH (const Type (&)[n]) { return n; }
/* A const version, but does not detect erratically being called on pointers. */
#define ARRAY_LENGTH_CONST(__array) ((signed int) (sizeof (__array) / sizeof (__array[0])))

#define _ASSERT_STATIC1(_line, _cond)	HB_UNUSED typedef int _static_assert_on_line_##_line##_failed[(_cond)?1:-1]
#define _ASSERT_STATIC0(_line, _cond)	_ASSERT_STATIC1 (_line, (_cond))
#define ASSERT_STATIC(_cond)		_ASSERT_STATIC0 (__LINE__, (_cond))


void fail (hb_bool_t suggest_help, const char *format, ...) G_GNUC_NORETURN G_GNUC_PRINTF (2, 3);


extern hb_bool_t debug;

struct option_group_t
{
  virtual void add_options (struct option_parser_t *parser) = 0;

  virtual void pre_parse (GError **error G_GNUC_UNUSED) {};
  virtual void post_parse (GError **error G_GNUC_UNUSED) {};
};


struct option_parser_t
{
  option_parser_t (const char *usage) {
    memset (this, 0, sizeof (*this));
    usage_str = usage;
    context = g_option_context_new (usage);
    to_free = g_ptr_array_new ();

    add_main_options ();
  }
  ~option_parser_t (void) {
    g_option_context_free (context);
    g_ptr_array_foreach (to_free, (GFunc) g_free, NULL);
    g_ptr_array_free (to_free, TRUE);
  }

  void add_main_options (void);

  void add_group (GOptionEntry   *entries,
		  const gchar    *name,
		  const gchar    *description,
		  const gchar    *help_description,
		  option_group_t *option_group);

  void free_later (char *p) {
    g_ptr_array_add (to_free, p);
  }

  void parse (int *argc, char ***argv);

  G_GNUC_NORETURN void usage (void) {
    g_printerr ("Usage: %s [OPTION...] %s\n", g_get_prgname (), usage_str);
    exit (1);
  }

  private:
  const char *usage_str;
  GOptionContext *context;
  GPtrArray *to_free;
};


#define DEFAULT_MARGIN 16
#define DEFAULT_FORE "#000000"
#define DEFAULT_BACK "#FFFFFF"
#define FONT_SIZE_UPEM 0x7FFFFFFF
#define FONT_SIZE_NONE 0

struct view_options_t : option_group_t
{
  view_options_t (option_parser_t *parser) {
    annotate = false;
    fore = NULL;
    back = NULL;
    line_space = 0;
    margin.t = margin.r = margin.b = margin.l = DEFAULT_MARGIN;

    add_options (parser);
  }
  ~view_options_t (void)
  {
    g_free (fore);
    g_free (back);
  }

  void add_options (option_parser_t *parser);

  hb_bool_t annotate;
  char *fore;
  char *back;
  double line_space;
  struct margin_t {
    double t, r, b, l;
  } margin;
};


struct shape_options_t : option_group_t
{
  shape_options_t (option_parser_t *parser)
  {
    direction = language = script = NULL;
    bot = eot = preserve_default_ignorables = false;
    features = NULL;
    num_features = 0;
    shapers = NULL;
    utf8_clusters = false;
    cluster_level = HB_BUFFER_CLUSTER_LEVEL_DEFAULT;
    normalize_glyphs = false;
    verify = false;
    num_iterations = 1;

    add_options (parser);
  }
  ~shape_options_t (void)
  {
    g_free (direction);
    g_free (language);
    g_free (script);
    free (features);
    g_strfreev (shapers);
  }

  void add_options (option_parser_t *parser);

  void setup_buffer (hb_buffer_t *buffer)
  {
    hb_buffer_set_direction (buffer, hb_direction_from_string (direction, -1));
    hb_buffer_set_script (buffer, hb_script_from_string (script, -1));
    hb_buffer_set_language (buffer, hb_language_from_string (language, -1));
    hb_buffer_set_flags (buffer, (hb_buffer_flags_t) (HB_BUFFER_FLAG_DEFAULT |
			 (bot ? HB_BUFFER_FLAG_BOT : 0) |
			 (eot ? HB_BUFFER_FLAG_EOT : 0) |
			 (preserve_default_ignorables ? HB_BUFFER_FLAG_PRESERVE_DEFAULT_IGNORABLES : 0)));
    hb_buffer_set_cluster_level (buffer, cluster_level);
    hb_buffer_guess_segment_properties (buffer);
  }

  static void copy_buffer_properties (hb_buffer_t *dst, hb_buffer_t *src)
  {
    hb_segment_properties_t props;
    hb_buffer_get_segment_properties (src, &props);
    hb_buffer_set_segment_properties (dst, &props);
    hb_buffer_set_flags (dst, hb_buffer_get_flags (src));
    hb_buffer_set_cluster_level (dst, hb_buffer_get_cluster_level (src));
  }

  void populate_buffer (hb_buffer_t *buffer, const char *text, int text_len,
			const char *text_before, const char *text_after)
  {
    hb_buffer_clear_contents (buffer);
    if (text_before) {
      unsigned int len = strlen (text_before);
      hb_buffer_add_utf8 (buffer, text_before, len, len, 0);
    }
    hb_buffer_add_utf8 (buffer, text, text_len, 0, text_len);
    if (text_after) {
      hb_buffer_add_utf8 (buffer, text_after, -1, 0, 0);
    }

    if (!utf8_clusters) {
      /* Reset cluster values to refer to Unicode character index
       * instead of UTF-8 index. */
      unsigned int num_glyphs = hb_buffer_get_length (buffer);
      hb_glyph_info_t *info = hb_buffer_get_glyph_infos (buffer, NULL);
      for (unsigned int i = 0; i < num_glyphs; i++)
      {
	info->cluster = i;
	info++;
      }
    }

    setup_buffer (buffer);
  }

  hb_bool_t shape (hb_font_t *font, hb_buffer_t *buffer, const char **error=NULL)
  {
    hb_buffer_t *text_buffer = NULL;
    if (verify)
    {
      text_buffer = hb_buffer_create ();
      hb_buffer_append (text_buffer, buffer, 0, -1);
    }

    if (!hb_shape_full (font, buffer, features, num_features, shapers))
    {
      if (error)
        *error = "all shapers failed.";
      return false;
    }

    if (normalize_glyphs)
      hb_buffer_normalize_glyphs (buffer);

    if (verify && !verify_buffer (buffer, text_buffer, font, error))
      return false;

    if (text_buffer)
      hb_buffer_destroy (text_buffer);

    return true;
  }

  bool verify_buffer (hb_buffer_t  *buffer,
		      hb_buffer_t  *text_buffer,
		      hb_font_t    *font,
		      const char  **error=NULL)
  {
    if (!verify_buffer_monotone (buffer, error))
      return false;
    if (!verify_buffer_safe_to_break (buffer, text_buffer, font, error))
      return false;
    return true;
  }

  bool verify_buffer_monotone (hb_buffer_t *buffer, const char **error=NULL)
  {
    /* Check that clusters are monotone. */
    if (cluster_level == HB_BUFFER_CLUSTER_LEVEL_MONOTONE_GRAPHEMES ||
	cluster_level == HB_BUFFER_CLUSTER_LEVEL_MONOTONE_CHARACTERS)
    {
      bool is_forward = HB_DIRECTION_IS_FORWARD (hb_buffer_get_direction (buffer));

      unsigned int num_glyphs;
      hb_glyph_info_t *info = hb_buffer_get_glyph_infos (buffer, &num_glyphs);

      for (unsigned int i = 1; i < num_glyphs; i++)
	if (info[i-1].cluster != info[i].cluster &&
	    (info[i-1].cluster < info[i].cluster) != is_forward)
	{
	  if (error)
	    *error = "clusters are not monotone.";
	  return false;
	}
    }

    return true;
  }

  bool verify_buffer_safe_to_break (hb_buffer_t  *buffer,
				    hb_buffer_t  *text_buffer,
				    hb_font_t    *font,
				    const char  **error=NULL)
  {
    if (cluster_level != HB_BUFFER_CLUSTER_LEVEL_MONOTONE_GRAPHEMES &&
	cluster_level != HB_BUFFER_CLUSTER_LEVEL_MONOTONE_CHARACTERS)
    {
      /* Cannot perform this check without monotone clusters.
       * Then again, unsafe-to-break flag is much harder to use without
       * monotone clusters. */
      return true;
    }

    /* Check that breaking up shaping at safe-to-break is indeed safe. */

    hb_buffer_t *fragment = hb_buffer_create ();
    hb_buffer_t *reconstruction = hb_buffer_create ();
    copy_buffer_properties (reconstruction, buffer);

    unsigned int num_glyphs;
    hb_glyph_info_t *info = hb_buffer_get_glyph_infos (buffer, &num_glyphs);

    unsigned int num_chars;
    hb_glyph_info_t *text = hb_buffer_get_glyph_infos (text_buffer, &num_chars);

    /* Chop text and shape fragments. */
    bool forward = HB_DIRECTION_IS_FORWARD (hb_buffer_get_direction (buffer));
    unsigned int start = 0;
    unsigned int text_start = forward ? 0 : num_chars;
    unsigned int text_end = text_start;
    for (unsigned int end = 1; end < num_glyphs + 1; end++)
    {
      if (end < num_glyphs &&
	  (info[end].cluster == info[end-1].cluster ||
	   info[end-(forward?0:1)].mask & HB_GLYPH_FLAG_UNSAFE_TO_BREAK))
	  continue;

      /* Shape segment corresponding to glyphs start..end. */
      if (end == num_glyphs)
      {
        if (forward)
	  text_end = num_chars;
	else
	  text_start = 0;
      }
      else
      {
	unsigned int cluster = info[end].cluster;
	if (forward)
	  while (text_end < num_chars && text[text_end].cluster != cluster)
	    text_end++;
	else
	  while (text_start && text[text_start - 1].cluster != cluster)
	    text_start--;
      }
      assert (text_start < text_end);

      //printf("start %d end %d text start %d end %d\n", start, end, text_start, text_end);
      hb_buffer_clear_contents (fragment);
      copy_buffer_properties (fragment, buffer);
      hb_buffer_append (fragment, text_buffer, text_start, text_end);
      if (!hb_shape_full (font, fragment, features, num_features, shapers))
      {
	if (error)
	  *error = "all shapers failed while shaping fragment.";
	hb_buffer_destroy (reconstruction);
	hb_buffer_destroy (fragment);
	return false;
      }
      hb_buffer_append (reconstruction, fragment, 0, -1);

      start = end;
      if (forward)
	text_start = text_end;
      else
	text_end = text_start;
    }

    hb_glyph_position_t *pos = hb_buffer_get_glyph_positions (buffer, NULL);

    unsigned int r_num_glyphs;
    hb_glyph_info_t *r_info = hb_buffer_get_glyph_infos (reconstruction, &r_num_glyphs);
    hb_glyph_position_t *r_pos = hb_buffer_get_glyph_positions (reconstruction, NULL);

    /* TODO Compare buffers. Remove assert. */
    assert (num_glyphs == r_num_glyphs);

    //hb_buffer_set_length (buffer, 0);
    //hb_buffer_append (buffer, reconstruction, 0, -1);

    hb_buffer_destroy (reconstruction);
    hb_buffer_destroy (fragment);

    return true;
  }

  void shape_closure (const char *text, int text_len,
		      hb_font_t *font, hb_buffer_t *buffer,
		      hb_set_t *glyphs)
  {
    hb_buffer_reset (buffer);
    hb_buffer_add_utf8 (buffer, text, text_len, 0, text_len);
    setup_buffer (buffer);
    hb_ot_shape_glyphs_closure (font, buffer, features, num_features, glyphs);
  }

  /* Buffer properties */
  char *direction;
  char *language;
  char *script;

  /* Buffer flags */
  hb_bool_t bot;
  hb_bool_t eot;
  hb_bool_t preserve_default_ignorables;

  hb_feature_t *features;
  unsigned int num_features;
  char **shapers;
  hb_bool_t utf8_clusters;
  hb_buffer_cluster_level_t cluster_level;
  hb_bool_t normalize_glyphs;
  hb_bool_t verify;
  unsigned int num_iterations;
};


struct font_options_t : option_group_t
{
  font_options_t (option_parser_t *parser,
		  int default_font_size_,
		  unsigned int subpixel_bits_)
  {
    variations = NULL;
    num_variations = 0;
    default_font_size = default_font_size_;
    subpixel_bits = subpixel_bits_;
    font_file = NULL;
    face_index = 0;
    font_size_x = font_size_y = default_font_size;
    font_funcs = NULL;

    font = NULL;

    add_options (parser);
  }
  ~font_options_t (void) {
    g_free (font_file);
    free (variations);
    g_free (font_funcs);
    hb_font_destroy (font);
  }

  void add_options (option_parser_t *parser);

  hb_font_t *get_font (void) const;

  char *font_file;
  int face_index;
  hb_variation_t *variations;
  unsigned int num_variations;
  int default_font_size;
  unsigned int subpixel_bits;
  mutable double font_size_x;
  mutable double font_size_y;
  char *font_funcs;

  private:
  mutable hb_font_t *font;
};


struct text_options_t : option_group_t
{
  text_options_t (option_parser_t *parser) {
    text_before = NULL;
    text_after = NULL;

    text = NULL;
    text_file = NULL;

    fp = NULL;
    gs = NULL;
    line = NULL;
    line_len = (unsigned int) -1;

    add_options (parser);
  }
  ~text_options_t (void) {
    g_free (text_before);
    g_free (text_after);
    g_free (text);
    g_free (text_file);
    if (gs)
      g_string_free (gs, true);
    if (fp)
      fclose (fp);
  }

  void add_options (option_parser_t *parser);

  void post_parse (GError **error G_GNUC_UNUSED) {
    if (text && text_file)
      g_set_error (error,
		   G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
		   "Only one of text and text-file can be set");
  };

  const char *get_line (unsigned int *len);

  char *text_before;
  char *text_after;

  char *text;
  char *text_file;

  private:
  FILE *fp;
  GString *gs;
  char *line;
  unsigned int line_len;
};

struct output_options_t : option_group_t
{
  output_options_t (option_parser_t *parser,
		    const char **supported_formats_ = NULL) {
    output_file = NULL;
    output_format = NULL;
    supported_formats = supported_formats_;
    explicit_output_format = false;

    fp = NULL;

    add_options (parser);
  }
  ~output_options_t (void) {
    g_free (output_file);
    g_free (output_format);
    if (fp)
      fclose (fp);
  }

  void add_options (option_parser_t *parser);

  void post_parse (GError **error G_GNUC_UNUSED)
  {
    if (output_format)
      explicit_output_format = true;

    if (output_file && !output_format) {
      output_format = strrchr (output_file, '.');
      if (output_format)
      {
	  output_format++; /* skip the dot */
	  output_format = strdup (output_format);
      }
    }

    if (output_file && 0 == strcmp (output_file, "-"))
      output_file = NULL; /* STDOUT */
  }

  FILE *get_file_handle (void);

  char *output_file;
  char *output_format;
  const char **supported_formats;
  bool explicit_output_format;

  mutable FILE *fp;
};

struct format_options_t : option_group_t
{
  format_options_t (option_parser_t *parser) {
    show_glyph_names = true;
    show_positions = true;
    show_clusters = true;
    show_text = false;
    show_unicode = false;
    show_line_num = false;
    show_extents = false;
    show_flags = false;
    trace = false;

    add_options (parser);
  }

  void add_options (option_parser_t *parser);

  void serialize_unicode (hb_buffer_t  *buffer,
			  GString      *gs);
  void serialize_glyphs (hb_buffer_t  *buffer,
			 hb_font_t    *font,
			 hb_buffer_serialize_format_t format,
			 hb_buffer_serialize_flags_t flags,
			 GString      *gs);
  void serialize_line_no (unsigned int  line_no,
			  GString      *gs);
  void serialize_buffer_of_text (hb_buffer_t  *buffer,
				 unsigned int  line_no,
				 const char   *text,
				 unsigned int  text_len,
				 hb_font_t    *font,
				 GString      *gs);
  void serialize_message (unsigned int  line_no,
			  const char   *msg,
			  GString      *gs);
  void serialize_buffer_of_glyphs (hb_buffer_t  *buffer,
				   unsigned int  line_no,
				   const char   *text,
				   unsigned int  text_len,
				   hb_font_t    *font,
				   hb_buffer_serialize_format_t output_format,
				   hb_buffer_serialize_flags_t format_flags,
				   GString      *gs);


  hb_bool_t show_glyph_names;
  hb_bool_t show_positions;
  hb_bool_t show_clusters;
  hb_bool_t show_text;
  hb_bool_t show_unicode;
  hb_bool_t show_line_num;
  hb_bool_t show_extents;
  hb_bool_t show_flags;
  hb_bool_t trace;
};

/* fallback implementation for scalbn()/scalbnf() for pre-2013 MSVC */
#if defined (_MSC_VER) && (_MSC_VER < 1800)

#ifndef FLT_RADIX
#define FLT_RADIX 2
#endif

__inline long double scalbn (long double x, int exp)
{
  return x * (pow ((long double) FLT_RADIX, exp));
}

__inline float scalbnf (float x, int exp)
{
  return x * (pow ((float) FLT_RADIX, exp));
}
#endif

#endif
