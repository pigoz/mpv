#include "af.h"
#include "talloc.h"
#include <libavresample/avresample.h>

struct af_lavr_priv {
    AVAudioResampleContext *avr;
};

static int control(struct af_instance_s *af, int cmd, void *arg)
{
    switch (cmd) {
        case AF_CONTROL_REINIT:

        memcpy(af->data,(af_data_t*)arg,sizeof(af_data_t));
        mp_msg(MSGT_AFILTER, MSGL_V, "[lavr] Was reinitialized: %iHz/%ich/%s\n",
        af->data->rate,af->data->nch,af_fmt2str_short(af->data->format));
            return AF_OK;
    }
    mp_msg(MSGT_AFILTER, MSGL_INFO, "[lavr] control ok.\n");
    return AF_UNKNOWN;
}

static void uninit(struct af_instance_s *af)
{
    mp_msg(MSGT_AFILTER, MSGL_INFO, "[lavr] uinit ok.\n");
    talloc_free(af->data);
}

static af_data_t* play(struct af_instance_s *af, struct af_data_s *data)
{
    mp_msg(MSGT_AFILTER, MSGL_INFO, "[lavr] play ok.\n");
    return data;
}

static int af_open(struct af_instance_s *af){
    af->control  = control;
    af->uninit   = uninit;
    af->play     = play;
    af->mul      = 1;
    af->data     = talloc_ptrtype(NULL, af->data);

    if (af->data == NULL) {
        mp_msg(MSGT_AFILTER, MSGL_ERR, "[lavr] initialization failed.\n");
        return AF_ERROR;
    }

    mp_msg(MSGT_AFILTER, MSGL_INFO, "[lavr] init OK.\n");
    return AF_OK;
}

af_info_t af_info_lavr_interleave = {
    "Audio samples interleaving filters based on libavresample",
    "lavr_interleave",
    "Stefano Pigozzi",
    "",
    AF_FLAGS_REENTRANT,
    af_open
};

af_info_t af_info_lavr = {
    "Audio resampling filters based on libavresample",
    "lavr",
    "Stefano Pigozzi",
    "",
    AF_FLAGS_REENTRANT,
    af_open
};
