// This file is part of OpenCV project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://opencv.org/license.html.

#ifndef __OPENCV_TEST_COMMON_HPP__
#define __OPENCV_TEST_COMMON_HPP__

#include "opencv2/dnn/utils/inference_engine.hpp"

#ifdef HAVE_OPENCL
#include "opencv2/core/ocl.hpp"
#endif

#define CV_TEST_TAG_DNN_SKIP_HALIDE              "dnn_skip_halide"
#define CV_TEST_TAG_DNN_SKIP_OPENCL              "dnn_skip_ocl"
#define CV_TEST_TAG_DNN_SKIP_OPENCL_FP16         "dnn_skip_ocl_fp16"
#define CV_TEST_TAG_DNN_SKIP_IE                  "dnn_skip_ie"
#define CV_TEST_TAG_DNN_SKIP_IE_2018R5           "dnn_skip_ie_2018r5"
#define CV_TEST_TAG_DNN_SKIP_IE_2019R1           "dnn_skip_ie_2019r1"
#define CV_TEST_TAG_DNN_SKIP_IE_2019R1_1         "dnn_skip_ie_2019r1_1"
#define CV_TEST_TAG_DNN_SKIP_IE_2019R2           "dnn_skip_ie_2019r2"
#define CV_TEST_TAG_DNN_SKIP_IE_2019R3           "dnn_skip_ie_2019r3"
#define CV_TEST_TAG_DNN_SKIP_IE_OPENCL           "dnn_skip_ie_ocl"
#define CV_TEST_TAG_DNN_SKIP_IE_OPENCL_FP16      "dnn_skip_ie_ocl_fp16"
#define CV_TEST_TAG_DNN_SKIP_IE_MYRIAD_2         "dnn_skip_ie_myriad2"
#define CV_TEST_TAG_DNN_SKIP_IE_MYRIAD_X         "dnn_skip_ie_myriadx"
#define CV_TEST_TAG_DNN_SKIP_IE_MYRIAD           CV_TEST_TAG_DNN_SKIP_IE_MYRIAD_2, CV_TEST_TAG_DNN_SKIP_IE_MYRIAD_X



namespace cv { namespace dnn {
CV__DNN_EXPERIMENTAL_NS_BEGIN

void PrintTo(const cv::dnn::Backend& v, std::ostream* os);
void PrintTo(const cv::dnn::Target& v, std::ostream* os);
using opencv_test::tuple;
using opencv_test::get;
void PrintTo(const tuple<cv::dnn::Backend, cv::dnn::Target> v, std::ostream* os);

CV__DNN_EXPERIMENTAL_NS_END
}} // namespace cv::dnn



namespace opencv_test {

void initDNNTests();

using namespace cv::dnn;

static inline const std::string &getOpenCVExtraDir()
{
    return cvtest::TS::ptr()->get_data_path();
}

void normAssert(
        cv::InputArray ref, cv::InputArray test, const char *comment = "",
        double l1 = 0.00001, double lInf = 0.0001);

std::vector<cv::Rect2d> matToBoxes(const cv::Mat& m);

void normAssertDetections(
        const std::vector<int>& refClassIds,
        const std::vector<float>& refScores,
        const std::vector<cv::Rect2d>& refBoxes,
        const std::vector<int>& testClassIds,
        const std::vector<float>& testScores,
        const std::vector<cv::Rect2d>& testBoxes,
        const char *comment = "", double confThreshold = 0.0,
        double scores_diff = 1e-5, double boxes_iou_diff = 1e-4);

// For SSD-based object detection networks which produce output of shape 1x1xNx7
// where N is a number of detections and an every detection is represented by
// a vector [batchId, classId, confidence, left, top, right, bottom].
void normAssertDetections(
        cv::Mat ref, cv::Mat out, const char *comment = "",
        double confThreshold = 0.0, double scores_diff = 1e-5,
        double boxes_iou_diff = 1e-4);

void readFileContent(const std::string& filename, CV_OUT std::vector<char>& content);

#ifdef HAVE_INF_ENGINE
bool validateVPUType();
#endif

testing::internal::ParamGenerator< tuple<Backend, Target> > dnnBackendsAndTargets(
        bool withInferenceEngine = true,
        bool withHalide = false,
        bool withCpuOCV = true
);


class DNNTestLayer : public TestWithParam<tuple<Backend, Target> >
{
public:
    dnn::Backend backend;
    dnn::Target target;
    double default_l1, default_lInf;

    DNNTestLayer()
    {
        backend = (dnn::Backend)(int)get<0>(GetParam());
        target = (dnn::Target)(int)get<1>(GetParam());
        getDefaultThresholds(backend, target, &default_l1, &default_lInf);
    }

    static void getDefaultThresholds(int backend, int target, double* l1, double* lInf)
    {
        if (target == DNN_TARGET_OPENCL_FP16 || target == DNN_TARGET_MYRIAD)
        {
            *l1 = 4e-3;
            *lInf = 2e-2;
        }
        else
        {
            *l1 = 1e-5;
            *lInf = 1e-4;
        }
    }

    static void checkBackend(int backend, int target, Mat* inp = 0, Mat* ref = 0)
    {
        if (backend == DNN_BACKEND_INFERENCE_ENGINE && target == DNN_TARGET_MYRIAD)
        {
            if (inp && ref && inp->dims == 4 && ref->dims == 4 &&
                inp->size[0] != 1 && inp->size[0] != ref->size[0])
            {
                applyTestTag(CV_TEST_TAG_DNN_SKIP_IE_MYRIAD);
                throw SkipTestException("Inconsistent batch size of input and output blobs for Myriad plugin");
            }
        }
    }

    void expectNoFallbacks(Net& net)
    {
        // Check if all the layers are supported with current backend and target.
        // Some layers might be fused so their timings equal to zero.
        std::vector<double> timings;
        net.getPerfProfile(timings);
        std::vector<String> names = net.getLayerNames();
        CV_Assert(names.size() == timings.size());

        for (int i = 0; i < names.size(); ++i)
        {
            Ptr<dnn::Layer> l = net.getLayer(net.getLayerId(names[i]));
            bool fused = !timings[i];
            if ((!l->supportBackend(backend) || l->preferableTarget != target) && !fused)
                CV_Error(Error::StsNotImplemented, "Layer [" + l->name + "] of type [" +
                         l->type + "] is expected to has backend implementation");
        }
    }

    void expectNoFallbacksFromIE(Net& net)
    {
        if (backend == DNN_BACKEND_INFERENCE_ENGINE)
            expectNoFallbacks(net);
    }

protected:
    void checkBackend(Mat* inp = 0, Mat* ref = 0)
    {
        checkBackend(backend, target, inp, ref);
    }
};

} // namespace


// src/op_inf_engine.hpp
#define INF_ENGINE_VER_MAJOR_GT(ver) (((INF_ENGINE_RELEASE) / 10000) > ((ver) / 10000))
#define INF_ENGINE_VER_MAJOR_GE(ver) (((INF_ENGINE_RELEASE) / 10000) >= ((ver) / 10000))
#define INF_ENGINE_VER_MAJOR_LT(ver) (((INF_ENGINE_RELEASE) / 10000) < ((ver) / 10000))
#define INF_ENGINE_VER_MAJOR_LE(ver) (((INF_ENGINE_RELEASE) / 10000) <= ((ver) / 10000))
#define INF_ENGINE_VER_MAJOR_EQ(ver) (((INF_ENGINE_RELEASE) / 10000) == ((ver) / 10000))

#endif
