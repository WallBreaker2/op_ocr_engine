
#include <tesseract/baseapi.h>
//#include <leptonica/allheaders.h>
#include <iostream>
#include <vector>
#include <tess_ocr.h>
const char* _stdcall ocr_engine_ver() {
    return "tess 5.3";
}
int  _stdcall ocr_engine_init(ocr_engine** obj, char** argv, int argc) {
    using std::cout;
    *obj = nullptr;
    if (argc != 3) {
        cout << "Usage:mainProgramName modelName languageName\n";
        return 1;
    }
    // ����Tesseract OCR����
    tesseract::TessBaseAPI* m_api = new tesseract::TessBaseAPI();
    // ��ʼ��Tesseract OCR
    if (m_api->Init(argv[1], argv[2])) {
        cout<<"Could not initialize tesseract.\n";
        delete m_api;
        return -1;
    }
    *obj = (ocr_engine*)m_api;
    return 0;
}
int  _stdcall ocr_engine_release(ocr_engine* obj) {
    if (obj) {
        // ɾ��Tesseract OCR����
        tesseract::TessBaseAPI* m_api = (tesseract::TessBaseAPI*)obj;
        delete m_api;
    }
    return 0;
}


int  _stdcall ocr_engine_ocr(ocr_engine* pocr, void* image, int w, int h, int bpp,
    ocr_engine_ocr_result** ppresult, int* num_of_result) {
    tesseract::TessBaseAPI* m_api = (tesseract::TessBaseAPI*)pocr;
    if (m_api == nullptr)return -1;
    
    // ��ͼ���������ø�Tesseract OCR
    m_api->SetImage((unsigned char*)image, w, h, bpp, w * bpp);

    // ִ������ʶ��
    m_api->Recognize(0);

    // ��ȡ���������
    tesseract::ResultIterator* ri = m_api->GetIterator();
    tesseract::PageIteratorLevel level = tesseract::RIL_WORD;
    //std::setlocale()
    // ��������������Ч����ѭ��
    if (ri != 0) {
        std::vector<ocr_engine_ocr_result> vres;
        do {
            // ��ȡʶ����ĵ��ʺ����Ŷ�
            const char* word = ri->GetUTF8Text(level);
            int x1, x2, y1, y2;
            ri->BoundingBox(level, &x1, &y1, &x2, &y2);
            float conf = ri->Confidence(level);
            ocr_engine_ocr_result res;
            res.x1 = x1;
            res.y1 = y1;
            res.x2 = x2;
            res.y2 = y2;
            res.text = (char*)malloc(strlen(word) + 1);
            strcpy(res.text, word);
            res.confidence = conf;
            
           
            // �ͷŵ��ʵ��ڴ�
            delete[] word;
            vres.push_back(res);
        } while (ri->Next(level));
        int n = vres.size();
        auto presult = (ocr_engine_ocr_result*)malloc(sizeof(ocr_engine_ocr_result) * n);
        if (presult == nullptr) {
            std::cout << "error in malloc\n";
            return -1;
        }
        memcpy(presult, &vres[0], n * sizeof(ocr_engine_ocr_result));
        *ppresult = presult;
        *num_of_result = n;
    }
    //Pix* px = m_api->GetInputImage();
    //pixWrite("test_oux.bmp", px, IFF_BMP);
    
    return *num_of_result;
}