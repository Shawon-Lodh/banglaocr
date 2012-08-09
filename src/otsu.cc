#include <colib/colib.h>
#include <iulib/iulib.h>
#include <ocropus/ocropus.h>

using namespace colib;
using namespace iulib;
using namespace ocropus;

// get a parameter from the environment, with a default value

param_string method("method","BinarizeByOtsu","binarization method");

int main(int argc,char **argv) {
    try {
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
    }
}

