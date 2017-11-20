
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>


#include <speex/speex.h>

static int dec_frame_size;
static int enc_frame_size;

static SpeexBits ebits, dbits;
void *enc_state;
void *dec_state;


int main(int argc, char *argv[])
{
    FILE         *fin, *fout;
    int           nin, tot_bytes;;
    char         *bin;
    short        *sout;

    if (argc < 2) {
	printf("usage: %s InputAudioBytesFile OutputRawFile \n", argv[0]);
	printf("e.g    %s pcm_bytes.spx pcm_s16.raw \n", argv[0]);
	exit(1);
    }

    if (strcmp(argv[1], "-")  == 0) fin = stdin;
    else if ( (fin = fopen(argv[1],"rb")) == NULL ) {
	fprintf(stderr, "Error opening input bytes file: %s: %s.\n",
         argv[1], strerror(errno));
	exit(1);
    }

    if (strcmp(argv[2], "-") == 0) fout = stdout;
    else if ( (fout = fopen(argv[2],"wb")) == NULL ) {
	fprintf(stderr, "Error opening output raw file: %s: %s.\n",
         argv[2], strerror(errno));
	exit(1);
    }


    speex_bits_init(&ebits);
	speex_bits_init(&dbits);
	int mode = 0;
	enc_state = speex_encoder_init(mode==0?&speex_nb_mode:&speex_wb_mode); 
	dec_state = speex_decoder_init(mode==0?&speex_nb_mode:&speex_wb_mode); 
	int tmp = 0;
	speex_encoder_ctl(enc_state, SPEEX_SET_QUALITY, &tmp);
	speex_encoder_ctl(enc_state, SPEEX_GET_FRAME_SIZE, &enc_frame_size);
	speex_decoder_ctl(dec_state, SPEEX_GET_FRAME_SIZE, &dec_frame_size);


    nin = 20;

    /* malloc some buffers that are dependant on Nc */

    bin = (char*)malloc(sizeof(char)*dec_frame_size); assert(bin != NULL);
    sout = (short*)malloc(sizeof(short)*dec_frame_size); assert(sout != NULL);
	tot_bytes = 0;

    while(fread(bin, sizeof(char), nin, fin) == nin)
    {
    	speex_bits_read_from(&dbits, bin, nin);
    	speex_decode_int(dec_state, &dbits, sout);   	
    	
        fwrite(sout, sizeof(short), dec_frame_size, fout);

		/* if this is in a pipeline, we probably don't want the usual
		   buffering to occur */

        if (fout == stdout) fflush(stdout);
        if (fin == stdin) fflush(stdin);
    }


    //fdmdv_dump_osc_mags(fdmdv);

    fclose(fin);
    fclose(fout);
    free(bin);
    free(sout);

    speex_bits_destroy(&ebits);
	speex_bits_destroy(&dbits);
	speex_decoder_destroy(dec_state); 
	speex_encoder_destroy(enc_state); 

    return 0;
}


