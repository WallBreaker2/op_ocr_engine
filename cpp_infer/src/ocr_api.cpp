
#include "opencv2/core.hpp"
#include "opencv2/imgcodecs.hpp"
#include "opencv2/imgproc.hpp"
#include <iostream>
#include <vector>

#include <include/args.h>
#include <include/paddleocr.h>
#include <include/paddlestructure.h>
#include <include/ocr_engine_api.h>



int check_params() {
	if (FLAGS_det) {
		if (FLAGS_det_model_dir.empty()) {
			std::cout << "Usage[det]: ./ppocr "
				"--det_model_dir=/PATH/TO/DET_INFERENCE_MODEL/ "
				<< "--image_dir=/PATH/TO/INPUT/IMAGE/" << std::endl;
			return 1;
		}
	}
	if (FLAGS_rec) {
		std::cout
			<< "In PP-OCRv3, rec_image_shape parameter defaults to '3, 48, 320',"
			"if you are using recognition model with PP-OCRv2 or an older "
			"version, "
			"please set --rec_image_shape='3,32,320"
			<< std::endl;
		if (FLAGS_rec_model_dir.empty()) {
			std::cout << "Usage[rec]: ./ppocr "
				"--rec_model_dir=/PATH/TO/REC_INFERENCE_MODEL/ "
				<< "--image_dir=/PATH/TO/INPUT/IMAGE/" << std::endl;
			return 1;
		}
	}
	if (FLAGS_cls && FLAGS_use_angle_cls) {
		if (FLAGS_cls_model_dir.empty()) {
			std::cout << "Usage[cls]: ./ppocr "
				<< "--cls_model_dir=/PATH/TO/REC_INFERENCE_MODEL/ "
				<< "--image_dir=/PATH/TO/INPUT/IMAGE/" << std::endl;
			return 1;
		}
	}
	if (FLAGS_table) {
		if (FLAGS_table_model_dir.empty() || FLAGS_det_model_dir.empty() ||
			FLAGS_rec_model_dir.empty()) {
			std::cout << "Usage[table]: ./ppocr "
				<< "--det_model_dir=/PATH/TO/DET_INFERENCE_MODEL/ "
				<< "--rec_model_dir=/PATH/TO/REC_INFERENCE_MODEL/ "
				<< "--table_model_dir=/PATH/TO/TABLE_INFERENCE_MODEL/ "
				<< "--image_dir=/PATH/TO/INPUT/IMAGE/" << std::endl;
			return 1;
		}
	}
	if (FLAGS_layout) {
		if (FLAGS_layout_model_dir.empty()) {
			std::cout << "Usage[layout]: ./ppocr "
				<< "--layout_model_dir=/PATH/TO/LAYOUT_INFERENCE_MODEL/ "
				<< "--image_dir=/PATH/TO/INPUT/IMAGE/" << std::endl;
			return 1;
		}
	}
	if (FLAGS_precision != "fp32" && FLAGS_precision != "fp16" &&
		FLAGS_precision != "int8") {
		std::cout << "precison should be 'fp32'(default), 'fp16' or 'int8'. "
			<< std::endl;
		return 1;
	}
	return ocr_engine_ok;
}



int _stdcall ocr_engine_init(ocr_engine** obj, char* argv[],int argc) {
	/*FLAGS_enable_mkldnn = true;
	FLAGS_det_model_dir = det_model_dir;
	FLAGS_rec_model_dir = rec_model_dir;
	FLAGS_rec_char_dict_path = rec_char_dict_path;*/
	google::ParseCommandLineFlags(&argc, &argv, false);
	if (check_params() == ocr_engine_ok) {
		*obj = reinterpret_cast<ocr_engine*>(new PaddleOCR::PPOCR());
		return ocr_engine_ok;
	}
	else {
		*obj = nullptr;
		return ocr_engine_err;
	}
	
}

int _stdcall ocr_engine_release(ocr_engine* obj) {
	using namespace PaddleOCR;
	if (obj) {
		PPOCR* p = reinterpret_cast<PPOCR*>(obj);
		delete p;
	}
	return ocr_engine_ok;
}


int _stdcall  ocr_engine_ocr(ocr_engine* pocr, void* image, int w, int h, int bpp,
	ocr_engine_ocr_result** ppresult, int* num_of_result) {
	using namespace PaddleOCR;
	*ppresult = nullptr;
	*num_of_result = 0;
	PPOCR& ocr = *reinterpret_cast<PPOCR*>(pocr);
	if (FLAGS_benchmark) {
		ocr.reset_timer();
	}
	cv::Mat mat = cv::Mat(h, w, bpp == 4 ? CV_8UC4 :
		bpp = 3 ? CV_8UC3 : CV_8UC1, image);
	cv::cvtColor(mat, mat, cv::IMREAD_COLOR);
	//cv::imwrite("paddle_ocr_test.bmp", mat);
	std::vector<OCRPredictResult> ocr_result =
		ocr.ocr(mat, FLAGS_det, FLAGS_rec, FLAGS_cls);
	auto presult = (ocr_engine_ocr_result*)malloc(sizeof(ocr_engine_ocr_result) * ocr_result.size());
	if (presult == nullptr) {
		std::cout << "error in malloc\n";
		return -1;
	}
	for (int i = 0; i < ocr_result.size(); i++) {
		presult[i].x1 = ocr_result[i].box[0][0];
		presult[i].y1 = ocr_result[i].box[0][1];
		presult[i].x2 = ocr_result[i].box[1][0];
		presult[i].y2 = ocr_result[i].box[1][1];
		
		presult[i].text  = (char*)malloc((ocr_result[i].text.length()+1) * sizeof(char));//
		strcpy(presult[i].text, ocr_result[i].text.c_str());
		presult[i].confidence = ocr_result[i].score;
	}
	*ppresult = presult;
	*num_of_result = ocr_result.size();
	if (FLAGS_visualize && FLAGS_det) {
		std::string file_name = Utility::basename("paddle_ocr_det.bmp");
		cv::Mat srcimg = mat;
		Utility::VisualizeBboxes(srcimg, ocr_result,
			FLAGS_output + "/" + file_name);
	}
	return 0;

}

