/*
 * Build-time codec check for the shipped GStreamer plugins.
 *
 * For every format games commonly use, make sure at least one real decoder or
 * demuxer (i.e. not a Proton media converter element) can be created from the
 * plugins in the dist directory. Fails the build if any is missing, so a codec
 * regression never makes it into a redist/deploy build.
 */

#include <gst/gst.h>
#include <stdio.h>
#include <string.h>

static const struct
{
    GstElementFactoryListType type;
    const char *caps;
    const char *what;
}
checks[] =
{
    /* audio */
    {GST_ELEMENT_FACTORY_TYPE_DECODER, "audio/x-wma, wmaversion=(int)2", "WMA v2 / xWMA (XACT, XAudio2)"},
    {GST_ELEMENT_FACTORY_TYPE_DECODER, "audio/x-wma, wmaversion=(int)3", "WMA Pro"},
    {GST_ELEMENT_FACTORY_TYPE_DECODER, "audio/mpeg, mpegversion=(int)4, stream-format=(string)raw", "AAC"},
    {GST_ELEMENT_FACTORY_TYPE_DECODER, "audio/mpeg, mpegversion=(int)1, layer=(int)3", "MP3"},
    {GST_ELEMENT_FACTORY_TYPE_DECODER, "audio/x-adpcm, layout=(string)microsoft", "MS ADPCM"},
    {GST_ELEMENT_FACTORY_TYPE_DECODER, "audio/x-vorbis", "Vorbis"},
    {GST_ELEMENT_FACTORY_TYPE_DECODER, "audio/x-opus", "Opus"},
    /* video */
    {GST_ELEMENT_FACTORY_TYPE_DECODER, "video/x-wmv, wmvversion=(int)3", "WMV3 (WMV9)"},
    {GST_ELEMENT_FACTORY_TYPE_DECODER, "video/x-wmv, wmvversion=(int)3, format=(string)WVC1", "VC-1"},
    {GST_ELEMENT_FACTORY_TYPE_DECODER, "video/x-h264, stream-format=(string)byte-stream, alignment=(string)au", "H.264"},
    {GST_ELEMENT_FACTORY_TYPE_DECODER, "video/x-h265, stream-format=(string)byte-stream, alignment=(string)au", "HEVC"},
    {GST_ELEMENT_FACTORY_TYPE_DECODER, "video/mpeg, mpegversion=(int)2, systemstream=(boolean)false", "MPEG-2"},
    {GST_ELEMENT_FACTORY_TYPE_DECODER, "video/mpeg, mpegversion=(int)4, systemstream=(boolean)false", "MPEG-4 Part 2"},
    {GST_ELEMENT_FACTORY_TYPE_DECODER, "video/x-vp8", "VP8"},
    {GST_ELEMENT_FACTORY_TYPE_DECODER, "video/x-theora", "Theora"},
    /* containers */
    {GST_ELEMENT_FACTORY_TYPE_DEMUXER, "video/x-ms-asf", "ASF / WMV container"},
    {GST_ELEMENT_FACTORY_TYPE_DEMUXER, "video/quicktime", "MP4 / MOV container"},
    {GST_ELEMENT_FACTORY_TYPE_DEMUXER, "video/x-msvideo", "AVI container"},
    {GST_ELEMENT_FACTORY_TYPE_DEMUXER, "video/x-matroska", "Matroska / WebM container"},
    {GST_ELEMENT_FACTORY_TYPE_DEMUXER, "video/mpeg, systemstream=(boolean)true", "MPEG-PS container"},
};

static gchar *find_real_element(GstElementFactoryListType type, GstCaps *caps)
{
    GList *factories, *filtered, *tmp;
    gchar *found = NULL;

    factories = gst_element_factory_list_get_elements(type, GST_RANK_MARGINAL);
    filtered = gst_element_factory_list_filter(factories, caps, GST_PAD_SINK, FALSE);
    gst_plugin_feature_list_free(factories);

    for (tmp = filtered; tmp && !found; tmp = tmp->next)
    {
        const gchar *name = gst_plugin_feature_get_name(GST_PLUGIN_FEATURE(tmp->data));
        GstElement *element;

        if (g_str_has_prefix(name, "proton"))
            continue;
        if ((element = gst_element_factory_create(GST_ELEMENT_FACTORY(tmp->data), NULL)))
        {
            found = g_strdup(name);
            gst_object_unref(element);
        }
    }

    gst_plugin_feature_list_free(filtered);
    return found;
}

int main(int argc, char **argv)
{
    unsigned int i, failures = 0;

    gst_init(&argc, &argv);

    for (i = 0; i < G_N_ELEMENTS(checks); ++i)
    {
        GstCaps *caps = gst_caps_from_string(checks[i].caps);
        gchar *name = find_real_element(checks[i].type, caps);

        if (name)
            printf("  ok    %-30s -> %s\n", checks[i].what, name);
        else
        {
            printf("  FAIL  %-30s (no element for %s)\n", checks[i].what, checks[i].caps);
            ++failures;
        }

        g_free(name);
        gst_caps_unref(caps);
    }

    if (failures)
        fprintf(stderr, "!! codec check: %u format(s) have no working decoder/demuxer\n", failures);
    return failures ? 1 : 0;
}
