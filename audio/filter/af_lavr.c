#include "af.h"
#include "talloc.h"
#include "audio/fmt-conversion.h"
#include <libavutil/opt.h>
#include <libavresample/avresample.h>
#include <libavutil/samplefmt.h>

struct af_lavr_priv {
    AVAudioResampleContext *avr;
    uint8_t *audio_data;
};

static void print_format(struct mp_audio *mpa, const char *message)
{
    mp_msg(MSGT_AFILTER, MSGL_V, "[lavr] %s: %iHz/%ich/%s\n",
        message, mpa->rate, mpa->nch, af_fmt2str_short(mpa->format));
}

static int control(struct af_instance *af, int cmd, void *arg)
{
    switch (cmd) {
        case AF_CONTROL_REINIT:
            memcpy(af->data, (struct mp_audio*)arg, sizeof(struct mp_audio));
            print_format(af->data, "REINT");
            return AF_OK;
    }

    mp_msg(MSGT_AFILTER, MSGL_INFO, "[lavr] control ok.%d\n", cmd);

    return AF_UNKNOWN;
}

static void uninit(struct af_instance *af)
{
    struct af_lavr_priv *p = af->setup;

    if (p->avr) avresample_free(&p->avr);
    talloc_free(p);
    talloc_free(af->data);
    mp_msg(MSGT_AFILTER, MSGL_INFO, "[lavr] uinit ok.\n");
}

static struct mp_audio *play(struct af_instance *af, struct mp_audio *data)
{
    struct af_lavr_priv *p = af->setup;
    struct mp_audio *in    = data;
    struct mp_audio *out   = af->data;
    uint8_t *in_data = data->audio;

    print_format(af->data, "play out sample_fmt");
    print_format(data,     "play in sample_fmt");

    int in_sample_rate     = in->rate;
    int out_sample_rate    = out->rate;

    int in_channel_layout  = av_get_default_channel_layout(in->nch);
    int out_channel_layout = av_get_default_channel_layout(out->nch);

    int in_sample_fmt      = afsamplefmt2avsamplefmt(in->format);
    int out_sample_fmt     = afsamplefmt2avsamplefmt(out->format);

    int nb_samples         = (in->len / 8) / in->bps;

    av_opt_set_int(p->avr, "in_channel_layout",  in_channel_layout,  0);
    av_opt_set_int(p->avr, "in_sample_fmt",      in_sample_fmt,      0);
    av_opt_set_int(p->avr, "in_sample_rate",     in_sample_rate,     0);

    av_opt_set_int(p->avr, "out_channel_layout", out_channel_layout, 0);
    av_opt_set_int(p->avr, "out_sample_fmt",     out_sample_fmt,     0);
    av_opt_set_int(p->avr, "out_sample_rate",    out_sample_rate,    0);

    if (avresample_open(p->avr) < 0) {
        mp_tmsg(MSGT_DECAUDIO, MSGL_ERR, "Failed to initialize libavresample"
                                         " context.\n");
        return data;
    }

    if (! p->audio_data) {
    mp_msg(MSGT_AFILTER, MSGL_INFO, "[lavr] samples: %i / %i = %i\n",
        in->len / 8, in->bps, (in->len / 8) / in->bps);
    }

    int out_linesize;
    int out_size   = av_samples_get_buffer_size(&out_linesize,
                                                out->nch,
                                                nb_samples,
                                                out_sample_fmt, 0);

    if (talloc_get_size(p->audio_data) != out_size)
        p->audio_data = talloc_realloc_size(p, p->audio_data, out_size);

    //int out_samples =
    //    avresample_convert(p->avr, &(p->audio_data), out_linesize, nb_samples,
    //                               &in_data, out_linesize, nb_samples);

    //if (out_samples < 0) {
    //    mp_tmsg(MSGT_DECAUDIO, MSGL_ERR, "Failed to resample packet.\n");
    //}

    //avresample_close(p->avr);

    //out->rate  = in->rate;
    //out->audio = p->audio_data;
    //out->len   = in->len;

    return in;
}

static int af_open(struct af_instance *af){
    af->control  = control;
    af->uninit   = uninit;
    af->play     = play;
    af->mul      = 1;
    af->data     = talloc_zero(NULL, struct mp_audio);

    struct af_lavr_priv *p = talloc_zero(NULL, struct af_lavr_priv);
    p->avr = avresample_alloc_context();
    af->setup = p;

    if (!p->avr) {
        mp_tmsg(MSGT_AFILTER, MSGL_ERR, "Failed to initialize libavresample"
                " context.\n");
        return AF_ERROR;
    }

    return AF_OK;
}

struct af_info af_info_lavr = {
    "Audio conversion filter based on libavresample",
    "lavr",
    "Stefano Pigozzi",
    "",
    AF_FLAGS_REENTRANT,
    af_open
};
