#define LOG_NDEBUG 0
#define LOG_TAG "exynosv4l2_test"
#include <cutils/log.h>

#include <fcntl.h>

#include <exynos_v4l2.h>

#define DEV_MEDIA "/dev/media1"
#define ME_NAME_SENSOR "mt9p111"
#define ME_NAME_FLITE_SD "flite-subdev.0"
#define ME_NAME_FLITE_VD "exynos-fimc-lite.0"

static int _mcSetupLink(media_device* m, const char* src, const char* snk)
{
    ALOGI("Setup link: %s->%s", src, snk);

    media_entity* meSrc = exynos_media_get_entity_by_name(m, src, strlen(src));
    if (meSrc == NULL) {
        ALOGE("Failed to get media entities for %s!", src);
        return -1;
    }

    media_entity* meSnk = exynos_media_get_entity_by_name(m, snk, strlen(snk));
    if (meSnk == NULL) {
        ALOGE("Failed to get media entities for %s!", snk);
        return -1;
    }

    for (unsigned int i = 0; i < meSrc->num_links; i++) {
        media_link* link = &(meSrc->links[i]);
        media_pad* srcPad = link->source;
        media_pad* snkPad = link->sink;

        if (srcPad->entity == meSrc && snkPad->entity == meSnk) {
            return exynos_media_setup_link(m, srcPad, snkPad,
                                           MEDIA_LNK_FL_ENABLED);
        }
    }

    ALOGE("Failed to link!");
    return -1;
}

static int setupMediaLinksForCamera(void)
{
    media_device* md = exynos_media_open(DEV_MEDIA);
    if (md == NULL) {
        ALOGE("failed to open %s!", DEV_MEDIA);
        return -1;
    }

    _mcSetupLink(md, ME_NAME_SENSOR, ME_NAME_FLITE_SD);
    _mcSetupLink(md, ME_NAME_FLITE_SD, ME_NAME_FLITE_VD);

    exynos_media_close(md);
    return 0;
}

static int openCamera(int ch)
{
    int fd = exynos_v4l2_open_devname(ME_NAME_FLITE_VD, O_RDWR);
    if (0 > fd) {
        ALOGE("Failed to open video dev. for %s!", ME_NAME_FLITE_VD);
        return -1;
    }

    int err = exynos_v4l2_s_input(fd, ch);
    if (0 > err) {
        ALOGE("Failed to set input ch. to %d", ch);
        goto err_out;
    }

err_out:
    exynos_v4l2_close(fd);
    return err;
}

#define DEV_VD_CAM "dev/video36"

int main(int argc, char* argv[])
{
    setupMediaLinksForCamera();
    openCamera(0);

    return 0;
}
