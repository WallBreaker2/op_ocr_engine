#pragma once
#define ocr_engine_ok 0
#define ocr_engine_err 1
struct ocr_engine;
struct ocr_engine_ocr_result {
	int x1, y1, x2, y2;
	char* text;
	float confidence;
};
#ifdef OCR_LIB_EXPORT
extern "C" {
	__declspec(dllexport) const char* _stdcall ocr_engine_ver();
	__declspec(dllexport) int  _stdcall ocr_engine_init(ocr_engine** obj, char* argv[], int argc);

	__declspec(dllexport) int  _stdcall ocr_engine_ocr(ocr_engine* pocr, void* image, int w, int h, int bpp,
		ocr_engine_ocr_result** ppresult, int* num_of_result);

	__declspec(dllexport) int  _stdcall ocr_engine_release(ocr_engine* obj);
}
#else
extern "C" {
	const char* _stdcall ocr_engine_ver();
	int  _stdcall ocr_engine_init(ocr_engine** obj, char* argv[], int argc);

	int  _stdcall ocr_engine_ocr(ocr_engine* pocr, void* image, int w, int h, int bpp,
		ocr_engine_ocr_result** ppresult, int* num_of_result);

	int  _stdcall ocr_engine_release(ocr_engine* obj);
}
#endif
