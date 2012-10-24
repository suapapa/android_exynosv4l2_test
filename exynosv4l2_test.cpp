#define LOG_NDEBUG 0
#define LOG_TAG "exynosv4l2_test"
#include <cutils/log.h>

#include <exynos_v4l2.h>


#define DEV_MEDIA "/dev/media1"
#define ME_NAME_SENSOR "mt9p111"
#define ME_NAME_FLITE_SD "flite-subdev.0"
#define ME_NAME_FLITE_VD "exynos-fimc-lite.0"

inline static media_entity* _mcGetEntity(media_device* m, char* n)
{
    return exynos_media_get_entity_by_name(m, n, strlen(n));
}

static int _mcSetupLink(media_device* m, media_entity* src, media_entity* sink)
{
    for (unsigned int i = 0; i < src->num_links; i++) {
        media_link* link = &(src->links[i]);
	media_pad *srcPad = link->source;
	media_pad *sinkPad = link->sink;

        if (link && srcPad->entity == src && sinkPad->entity == sink) {
            return exynos_media_setup_link(m, srcPad, sinkPad,
                                           MEDIA_LNK_FL_ENABLED);
        }
    }

    ALOGE("Failed to link!");
    return -1;
}

static int mcSetupLinks(void)
{
    media_device* md = exynos_media_open(DEV_MEDIA);
    if (md == NULL) {
        ALOGE("failed to open %s", DEV_MEDIA);
        return -1;
    }

    media_entity* meSensor = _mcGetEntity(md, ME_NAME_SENSOR);
    media_entity* meFliteSd = _mcGetEntity(md, ME_NAME_FLITE_SD);
    media_entity* meFliteVd = _mcGetEntity(md, ME_NAME_FLITE_VD);

    mcSetupLink(md, meSensor, meFliteSd);
    mcSetupLink(md, meFliteSd, meFliteVd);

    exynos_media_close(md);
    return 0;
}

#define DEV_VD_CAM "dev/video36"

int main(int argc, char* argv[])
{
    mcSetupLinks();
    return 0;
}
