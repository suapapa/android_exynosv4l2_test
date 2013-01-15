#define LOG_NDEBUG 0
#define LOG_TAG "exynosv4l2_test"
#include <cutils/log.h>
#include <ion.h>

#include <stdio.h>
#include <fcntl.h>

#include <exynos_v4l2.h>

/* Choose one */
/* #define USE_SENSOR_MT9P111 */
#define USE_SENSOR_S5K3H5
/* #define USE_SENSOR_S5K4ECGX */

#define DEV_MEDIA "/dev/media1"

#ifdef USE_SENSOR_MT9P111
#define ME_NAME_SENSOR "mt9p111"
#endif

#ifdef USE_SENSOR_S5K4ECGX
#define ME_NAME_SENSOR "S5K4ECGX 4-003c"
#endif

#ifdef USE_SENSOR_S5K3H5
#define ME_NAME_SENSOR "s5k3h5"
#endif

#define ME_NAME_CSIS_SD "s5p-mipi-csis.0"
#define ME_NAME_FLITE_SD "flite-subdev.0"
#define ME_NAME_FLITE_VD "exynos-fimc-lite.0"
#define ME_NAME_GSC_SD "gsc-cap-subdev.0"
#define ME_NAME_GSC_VD "exynos-gsc.0.capture"

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

#ifndef USE_SENSOR_MT9P111
    _mcSetupLink(md, ME_NAME_SENSOR, ME_NAME_CSIS_SD);
    _mcSetupLink(md, ME_NAME_CSIS_SD, ME_NAME_FLITE_SD);
#else
    _mcSetupLink(md, ME_NAME_SENSOR, ME_NAME_FLITE_SD);
#endif
    _mcSetupLink(md, ME_NAME_FLITE_SD, ME_NAME_FLITE_VD);
    _mcSetupLink(md, ME_NAME_FLITE_SD, ME_NAME_GSC_SD);
    _mcSetupLink(md, ME_NAME_GSC_SD, ME_NAME_GSC_VD);

    exynos_media_close(md);
    return 0;
}

#define EXYNOSV4L2_TEST_NUM_PLANES 1
#define MAX_CAM_BUF_CNT 4

static ion_client gIonClient = NULL;
static ion_buffer gIonBuffer[MAX_CAM_BUF_CNT] = {NULL, };

static char* gBuffer[MAX_CAM_BUF_CNT] = {NULL, };
static unsigned int gBufferSize = 0;
static unsigned int gBufferCnt = 0;

static void deinitIonBufs(void);
static void initIonBufs(unsigned int size, unsigned int cnt)
{
    if (gBufferSize || gBufferCnt || gIonClient) {
        deinitIonBufs();
    }

    ALOGI("creating ion client...");
    gIonClient = ion_client_create();

    gBufferSize = size;
    gBufferCnt = cnt;

    ALOGI("alloc and mapping %d buffers of %d bytes each...", cnt, size);
    for (unsigned int i = 0; i < cnt; i++) {
        gIonBuffer[i] = ion_alloc(gIonClient, size, 4 * 1024,
			/* ION_HEAP_EXYNOS_VIDEO_MASK */(ION_HEAP_EXYNOS_CONTIG_MASK | (1 << (32-3))));
        gBuffer[i] = (char*)ion_map(gIonBuffer[i], size, 0);
    }
}

static void deinitIonBufs(void)
{
    ALOGI("unmap and freeing ion buffers...");
    for (unsigned int i = 0; i < gBufferCnt; i++) {
        ion_unmap(gBuffer[i], gBufferSize);
        ion_free(gIonBuffer[i]);

    }
    gBufferSize = 0;
    gBufferCnt = 0;

    ALOGI("destroying ion client...");
    ion_client_destroy(gIonClient);
    gIonClient = 0;
}

static int qBuf(int fd, int index)
{
    struct v4l2_buffer v4l2_buf;
    struct v4l2_plane planes[1];
    v4l2_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    v4l2_buf.memory = V4L2_MEMORY_USERPTR;
    v4l2_buf.m.planes = planes;
    v4l2_buf.length = 1;
    v4l2_buf.index = index;

    v4l2_buf.m.planes[0].m.userptr = (unsigned long)gBuffer[index];
    v4l2_buf.m.planes[0].length = gBufferSize;

    ALOGI("%s: idx=%d, userptr=0x%p", __func__,
          index, (void*)v4l2_buf.m.planes[0].m.userptr);

    return exynos_v4l2_qbuf(fd, &v4l2_buf);
}

static int dqBuf(int fd)
{
    struct v4l2_buffer v4l2_buf;
    struct v4l2_plane planes[1];
    v4l2_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    v4l2_buf.memory = V4L2_MEMORY_USERPTR;
    v4l2_buf.m.planes = planes;
    v4l2_buf.length = 1;

    int ret = exynos_v4l2_dqbuf(fd, &v4l2_buf);
    if (ret < 0)
        ALOGE("%s: failed! ret=%d", __func__, ret);

    return v4l2_buf.index;
}

static int writeCapturedBuf(int fd, int idx)
{
    int bufIdx = dqBuf(fd);
    if (0 > bufIdx) {
        ALOGW("Failed to dqBuf");
        return 0;
    }

    // Write buf to file
    char fileName[100];
    sprintf(fileName, "camera_capture_%d.dump", idx);
    FILE* wd = fopen(fileName, "wb");
    if (!wd) {
        ALOGE("Failed to open %s", fileName);
        goto exit_capture;
    }

    ALOGI("Write buffer %d to %s.", idx, fileName);
    fwrite(gBuffer[bufIdx], 1, gBufferSize, wd);

exit_capture:
    if (wd)
        fclose(wd);

    return qBuf(fd, bufIdx);
}


static int setSuvdevFmt(const char* name, int pad, int w, int h, int fcode)
{
    int sd = exynos_subdev_open_devname(name, O_RDWR);
    struct v4l2_subdev_format v4l2_sd_fmt;
    v4l2_sd_fmt.pad = pad;
    v4l2_sd_fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
    v4l2_sd_fmt.format.width = w;
    v4l2_sd_fmt.format.height = h;
    v4l2_sd_fmt.format.code = fcode;
    int ret = exynos_subdev_s_fmt(sd, &v4l2_sd_fmt);
    if (0 > ret) {
        ALOGE("Failed to set subdev fmt");
        return -1;
    }

    return exynos_subdev_close(sd);
    /* return 0; */
}

static int openCamera(int capCnt)
{
    int w, h, ret;
    int ion_fd = 0;
    struct ion_handle* ion_hdl = NULL;



    ALOGV("open...");
    int fd = exynos_v4l2_open_devname(ME_NAME_FLITE_VD, O_RDWR);
    if (0 > fd) {
        ALOGE("Failed to open video dev. for %s!", ME_NAME_FLITE_VD);
        return -1;
    }

    ALOGV("s_input...");
    ret = exynos_v4l2_s_input(fd, 0);
    if (0 > ret) {
        ALOGE("Failed to set input ch. to %d", 0);
        goto err_out;
    }

    ALOGV("s_fmt...");
#ifdef USE_SENSOR_S5K3H5
    w = 816;
    h = 612;
#else
    w = 640;
    h = 480;
#endif

    setSuvdevFmt(ME_NAME_SENSOR, 0, w, h, V4L2_MBUS_FMT_YUYV8_2X8);
    setSuvdevFmt(ME_NAME_CSIS_SD, 0, w, h, V4L2_MBUS_FMT_YUYV8_2X8);
    setSuvdevFmt(ME_NAME_CSIS_SD, 1, w, h, V4L2_MBUS_FMT_YUYV8_2X8);
    setSuvdevFmt(ME_NAME_FLITE_SD, 0, w, h, V4L2_MBUS_FMT_YUYV8_2X8);

    struct v4l2_format v4l2_fmt;
    memset(&v4l2_fmt, 0, sizeof(struct v4l2_format));
    v4l2_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    v4l2_fmt.fmt.pix_mp.num_planes = EXYNOSV4L2_TEST_NUM_PLANES;
    v4l2_fmt.fmt.pix_mp.width = w;
    v4l2_fmt.fmt.pix_mp.height = h;
    v4l2_fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_YUYV;
    v4l2_fmt.fmt.pix_mp.field = V4L2_FIELD_ANY;
    ret = exynos_v4l2_s_fmt(fd, &v4l2_fmt);
    if (0 > ret) {
        ALOGE("Failed to s_fmt %dx%d", w, h);
        goto err_out;
    }

    ALOGV("reqbufs...");
    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count = MAX_CAM_BUF_CNT;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    req.memory = V4L2_MEMORY_USERPTR /* V4L2_MEMORY_MMA */;
    ret = exynos_v4l2_reqbufs(fd, &req);
    if (0 > ret) {
        ALOGE("Failed to reqbufs. ret = %d", ret);
        goto err_out;
    }
    ALOGI("reqbufs count = %d", req.count);

    initIonBufs(w * h * 2, MAX_CAM_BUF_CNT);

    ALOGV("qbuf all...");
    for (int i = 0; i < MAX_CAM_BUF_CNT; i++) {
        if (0 > qBuf(fd, i)) {
            ALOGE("Failed to qbuf, %d", i);
            goto err_out;
        }
    }

    ALOGV("streaming...");
    ret = exynos_v4l2_streamon(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
    if (ret < 0)
        ALOGE("Failed to start stream. ret=%d", ret);

    ALOGV("capture %d buffers", capCnt);
    for (int i = 0; i < capCnt; i++) {
        ALOGV("capturing %d...", i + 1);
        writeCapturedBuf(fd, i);
    }

    ret = exynos_v4l2_streamoff(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
    if (ret < 0)
        ALOGE("Failed to stop stream. ret=%d", ret);

err_out:
    deinitIonBufs();
    exynos_v4l2_close(fd);
    return ret;
}

int main(int argc, char* argv[])
{
    setupMediaLinksForCamera();
    openCamera(10);

    return 0;
}
