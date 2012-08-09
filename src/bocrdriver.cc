#include <iostream>
#include <colib/colib.h>
#include <iulib/iulib.h>
#include <ocropus/ocropus.h>
//#include <ocropus/tesseract.h>
#include "bocr.h"


using namespace colib;
using namespace iulib;
using namespace ocropus;
using namespace banglaocr;
using namespace std;

// get a parameter from the environment, with a default value

//param_string method("method","BinarizeByOtsu","binarization method");

int main(int argc,char **argv) {
    /*try {
        if(argc!=3) throw "wrong # arguments";

        // register all the internal OCRopus components so that make_component works
        init_ocropus_components();

        // instantiate the binarization component
        autodel<IBinarize> binarizer;
        make_component(method,binarizer);

        // read an input image
        bytearray image;
        read_image_gray(image,argv[1]);

        // apply the binarizer
        bytearray output;
        binarizer->binarize(output,image);

        // write the result
        write_image_gray(argv[2],output);
    } catch(const char *message) {
        fprintf(stderr,"error: %s\n",message);
        exit(1);
    }*/
    intarray result_segmentation,page_segmentation_tess;
    bytearray orig_image,page_image_tess;
    //RecognizedPage p;
    //FILE* fin=fopen(argv[1],"rb");
    read_image_gray(orig_image,argv[1]);
    //read_image_gray(orig_image,fin,"png");
    segmentUnitsOfBlockImage(result_segmentation, orig_image);
    //simple_recolor(result_segmentation);
    createTesseractFormatImage(result_segmentation, page_image_tess, page_segmentation_tess);
    //write_image_packed(argv[2],result_segmentation);
    //FILE* fout=fopen(argv[2],"wb");
    //write_image_gray(fout,page_image_tess,"jpg");
    write_image_gray(argv[2],page_image_tess);
}

