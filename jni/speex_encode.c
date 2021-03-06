
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
    short        *sin;
    char         *bout;

    if (argc < 2) {
	printf("usage: %s InputAudioRawFile OutputBytesFile \n", argv[0]);
	printf("e.g    %s pcm_s16.raw pcm_bytes.spx\n", argv[0]);
	exit(1);
    }

    if (strcmp(argv[1], "-")  == 0) fin = stdin;
    else if ( (fin = fopen(argv[1],"rb")) == NULL ) {
	fprintf(stderr, "Error opening input raw sample file: %s: %s.\n",
         argv[1], strerror(errno));
	exit(1);
    }

    if (strcmp(argv[2], "-") == 0) fout = stdout;
    else if ( (fout = fopen(argv[2],"wb")) == NULL ) {
	fprintf(stderr, "Error opening output bytes file: %s: %s.\n",
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


    nin = enc_frame_size;

    /* malloc some buffers that are dependant on Nc */

    sin = (short*)malloc(sizeof(short)*enc_frame_size); assert(sin != NULL);
    bout = (char*)malloc(sizeof(char)*enc_frame_size); assert(bout != NULL);
	tot_bytes = 0;

    while(fread(sin, sizeof(short), nin, fin) == nin)
    {
    	speex_bits_reset(&ebits);
    	speex_encode_int(enc_state, sin, &ebits);
    	tot_bytes = speex_bits_write(&ebits, (char *)bout, enc_frame_size);    	
    	
        fwrite(bout, sizeof(char), tot_bytes, fout);

		/* if this is in a pipeline, we probably don't want the usual
		   buffering to occur */

        if (fout == stdout) fflush(stdout);
        if (fin == stdin) fflush(stdin);
    }


    //fdmdv_dump_osc_mags(fdmdv);

    fclose(fin);
    fclose(fout);
    free(sin);
    free(bout);

    speex_bits_destroy(&ebits);
	speex_bits_destroy(&dbits);
	speex_decoder_destroy(dec_state); 
	speex_encoder_destroy(enc_state); 

    return 0;
}


