
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
    int           i, bit, byte, c;
    int           nin, nin_prev;
    int           sync_bit = 0, reliable_sync_bit;
    int           sync = 0;
    int           f;
    FILE         *foct = NULL;
    struct MODEM_STATS stats;
    COMP         *rx_fdm_log;
    int           rx_fdm_log_col_index;
    COMP         *rx_symbols_log;
    int           sync_log[MAX_FRAMES];
    float         rx_timing_log[MAX_FRAMES];
    float         foff_log[MAX_FRAMES];
    int           sync_bit_log[MAX_FRAMES];
    int           rx_bits_log[FDMDV_BITS_PER_FRAME*MAX_FRAMES];
    float         snr_est_log[MAX_FRAMES];
    float        *rx_spec_log;
    int           max_frames_reached;
    int           bits_per_fdmdv_frame;
    int           bits_per_codec_frame;
    int           bytes_per_codec_frame;

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


    fdmdv = fdmdv_create(Nc);
    modem_stats_open(&stats);

    bits_per_fdmdv_frame = fdmdv_bits_per_frame(fdmdv);
    bits_per_codec_frame = 2*fdmdv_bits_per_frame(fdmdv);
    bytes_per_codec_frame = (bits_per_codec_frame+7)/8;

    /* malloc some buffers that are dependant on Nc */

    packed_bits = (char*)malloc(bytes_per_codec_frame); assert(packed_bits != NULL);
    rx_bits = (int*)malloc(sizeof(int)*bits_per_codec_frame); assert(rx_bits != NULL);
    codec_bits = (int*)malloc(2*sizeof(int)*bits_per_fdmdv_frame); assert(codec_bits != NULL);

    /* malloc some of the larger variables to prevent out of stack problems */

    rx_fdm_log = (COMP*)malloc(sizeof(COMP)*FDMDV_MAX_SAMPLES_PER_FRAME*MAX_FRAMES);
    assert(rx_fdm_log != NULL);
    rx_spec_log = (float*)malloc(sizeof(float)*MODEM_STATS_NSPEC*MAX_FRAMES);
    assert(rx_spec_log != NULL);
    rx_symbols_log = (COMP*)malloc(sizeof(COMP)*(Nc+1)*MAX_FRAMES);
    assert(rx_fdm_log != NULL);

    f = 0;
    nin = FDMDV_NOM_SAMPLES_PER_FRAME;
    rx_fdm_log_col_index = 0;
    max_frames_reached = 0;

    while(fread(rx_fdm_scaled, sizeof(short), nin, fin) == nin)
    {
	for(i=0; i<nin; i++) {
	    rx_fdm[i].real = (float)rx_fdm_scaled[i]/FDMDV_SCALE;
            rx_fdm[i].imag = 0;
        }
	nin_prev = nin;
	fdmdv_demod(fdmdv, rx_bits, &reliable_sync_bit, rx_fdm, &nin);

	/* log data for optional Octave dump */

	if (f < MAX_FRAMES) {
	    fdmdv_get_demod_stats(fdmdv, &stats);

	    /* log modem states for later dumping to Octave log file */

	    memcpy(&rx_fdm_log[rx_fdm_log_col_index], rx_fdm, sizeof(COMP)*nin_prev);
	    rx_fdm_log_col_index += nin_prev;

	    for(c=0; c<Nc+1; c++)
		rx_symbols_log[f*(Nc+1)+c] = stats.rx_symbols[0][c];
	    foff_log[f] = stats.foff;
	    rx_timing_log[f] = stats.rx_timing;
	    sync_log[f] = stats.sync;
	    sync_bit_log[f] = sync_bit;
	    memcpy(&rx_bits_log[bits_per_fdmdv_frame*f], rx_bits, sizeof(int)*bits_per_fdmdv_frame);
	    snr_est_log[f] = stats.snr_est;

	    modem_stats_get_rx_spectrum(&stats, &rx_spec_log[f*MODEM_STATS_NSPEC], rx_fdm, nin_prev);

	    f++;
	}

	if ((f == MAX_FRAMES) && !max_frames_reached) {
	    fprintf(stderr,"MAX_FRAMES exceed in Octave log, log truncated\n");
	    max_frames_reached = 1;
	}

        if (reliable_sync_bit)
            sync = 1;
        //printf("sync_bit: %d reliable_sync_bit: %d sync: %d\n", sync_bit, reliable_sync_bit, sync);

        if (sync == 0) {
            memcpy(codec_bits, rx_bits, bits_per_fdmdv_frame*sizeof(int));
            sync = 1;
        }
        else {
            memcpy(&codec_bits[bits_per_fdmdv_frame], rx_bits, bits_per_fdmdv_frame*sizeof(int));

            /* pack bits, MSB received first  */

            bit = 7; byte = 0;
            memset(packed_bits, 0, bytes_per_codec_frame);
            for(i=0; i<bits_per_codec_frame; i++) {
                packed_bits[byte] |= (codec_bits[i] << bit);
                bit--;
                if (bit < 0) {
                    bit = 7;
                    byte++;
                }
            }

            fwrite(packed_bits, sizeof(char), bytes_per_codec_frame, fout);
            sync = 0;
        }


	/* if this is in a pipeline, we probably don't want the usual
	   buffering to occur */

        if (fout == stdout) fflush(stdout);
        if (fin == stdin) fflush(stdin);
    }

    /* Optional dump to Octave log file */

    if (argc == 5) {

	if ((foct = fopen(argv[4],"wt")) == NULL ) {
	    fprintf(stderr, "Error opening Octave dump file: %s: %s.\n",
		argv[4], strerror(errno));
	    exit(1);
	}
	octave_save_complex(foct, "rx_fdm_log_c", rx_fdm_log, 1, rx_fdm_log_col_index, FDMDV_MAX_SAMPLES_PER_FRAME);
	octave_save_complex(foct, "rx_symbols_log_c", (COMP*)rx_symbols_log, Nc+1, f, MAX_FRAMES);
	octave_save_float(foct, "foff_log_c", foff_log, 1, f, MAX_FRAMES);
	octave_save_float(foct, "rx_timing_log_c", rx_timing_log, 1, f, MAX_FRAMES);
	octave_save_int(foct, "sync_log_c", sync_log, 1, f);
	octave_save_int(foct, "rx_bits_log_c", rx_bits_log, 1, bits_per_fdmdv_frame*f);
	octave_save_int(foct, "sync_bit_log_c", sync_bit_log, 1, f);
	octave_save_float(foct, "snr_est_log_c", snr_est_log, 1, f, MAX_FRAMES);
	octave_save_float(foct, "rx_spec_log_c", rx_spec_log, f, MODEM_STATS_NSPEC, MODEM_STATS_NSPEC);
	fclose(foct);
    }

    //fdmdv_dump_osc_mags(fdmdv);

    fclose(fin);
    fclose(fout);
    free(rx_fdm_log);
    free(rx_spec_log);
    fdmdv_destroy(fdmdv);

    return 0;
}


