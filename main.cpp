//
// Vinyl control external, using the timecoder from
// Mark Hill's xwax-src <http://www.xwax.co.uk>
//
// Niklas Klügel <kluegel@in.tum.de>

#include <flext.h>

extern "C" {
    #include "timecoder.h"
}

#define SCALE ((float)(1<<15))
#define SMOOTHING 10

#if !defined(FLEXT_VERSION) || (FLEXT_VERSION < 401)
#error You need at least flext version 0.4.1
#endif

class vinylcontrol: public flext_dsp {


	FLEXT_HEADER_S(vinylcontrol, flext_dsp, setup)

	public:
		vinylcontrol(int argc, t_atom* argv):hasBeenAlive(0), pitchfactor(1.0), smoothingIterator(0), pitchAVG(0.0) {

                    const char* vinylType;
                    if(argc == 2) {
                        vinylType = GetString(argv[0]);
                        smoothingIterations = GetInt(argv[1]);
                    } else if(argc == 1) {
                        vinylType = GetString(argv[0]);
                    } else {
                        post("%s - no arguments given",thisName());
                        vinylType = "serato_2a";
                        smoothingIterations = SMOOTHING;
                    }

                    if(timecoder_build_lookup((char*)vinylType) == -1) {
                        post("Error building lookup: You may have given an unknown vinyl-type as parameter.");
                        InitProblem();
                    }
                    timecoder_init(&tc);

                    lastTimecode = (int)timecoder_get_safe(&tc);
                    firstTimecode = 0;
                    currentTimecode = 0;


	            AddInSignal("left audio in");       // left audio in
		    AddInSignal("right audio in");      // right audio in
                    AddInAnything("Reset start of timecode and set vinyl type and smoothing");
		    //AddInSymbol("vinyl type: serato_2a, serato_2b, serato_cd, traktor_a, traktor_b");
                    AddOutBang("Alive");
		    AddOutFloat("Pitch");
                    AddOutFloat("Position");

		    post("-- vinylcontrol~ ---\n Arguments:\n1. vinyl-type: serato_2a, serato_2b, serato_cd, traktor_a, traktor_b\n    using: %s\n2. smoothing (averaging pitch and alive-events), default is %d\n    using %d", vinylType,SMOOTHING, smoothingIterations);
		} // end of constructor


	protected:
		virtual void m_signal(int n, float *const *in, float *const *out);

                float m_checkSamplerateFactor();
                void m_output_values();

                void m_resetFirstTimecode();
                void m_setVinylType(int argc, t_atom *argv){
                    if (argc == 0) return;
                    const char * vinylType = GetString(argv[0]);
                    post("set vinyl type to %s", vinylType);
                    if(timecoder_build_lookup((char*)vinylType) == -1) {
                        post("Error building lookup: You may have given an unknown vinyl-type as parameter.");
                    }
                };

                void m_setSmoothing(int i){
                    post("set smoothing to %d", i);
                    smoothingIterations = i;
                }

	private:
                static void setup(t_classid c){
                    FLEXT_CADDBANG(c,2,m_resetFirstTimecode);
                    FLEXT_CADDMETHOD_(c,2,"smoothing", m_setSmoothing);
                    FLEXT_CADDMETHOD_(c,2,"vinyl", m_setVinylType);
                }

                FLEXT_CALLBACK(m_resetFirstTimecode)
                FLEXT_CALLBACK_V(m_setVinylType)
                FLEXT_CALLBACK_I(m_setSmoothing)

                float pitch;
                float pitchfactor;

                float pitchAVG;

                unsigned int smoothingIterator;
                unsigned int smoothingIterations;


                int lastTimecode;
                int firstTimecode;
                int currentTimecode;

                int hasBeenAlive;
                struct timecoder_t tc;

                signed short pcm[8192];

};


FLEXT_NEW_DSP_V("vinylcontrol~", vinylcontrol)

inline float vinylcontrol::m_checkSamplerateFactor() {
        return Samplerate() / float(DEVICE_RATE);
}


void vinylcontrol::m_resetFirstTimecode() {
    firstTimecode = 0;
}

void vinylcontrol::m_output_values() {
    if(timecoder_get_alive(&tc)) {
            if(hasBeenAlive == 0 && (smoothingIterator%smoothingIterations == 0)) {
                hasBeenAlive = 1;
                ToOutBang(0);
            }
            timecoder_get_pitch(&tc, &pitch);
         //   ToOutFloat(1, pitch*m_checkSamplerateFactor());

            if(smoothingIterator % smoothingIterations != 0) {
                pitchAVG += pitch;
            } else {
                pitch = pitchAVG/smoothingIterations;
                ToOutFloat(1, pitch*m_checkSamplerateFactor());
                pitchAVG = pitch;
            }

            if((currentTimecode = timecoder_get_position(&tc, NULL)) != -1) {
                if(firstTimecode == 0) {
                    firstTimecode = currentTimecode;
                }
                ToOutFloat(2, (((float)(currentTimecode-firstTimecode))/float(lastTimecode-firstTimecode)));
            }

    } else if (hasBeenAlive == 1 && (smoothingIterator%smoothingIterations == 0)) {
            hasBeenAlive = 0;
            ToOutBang(0);
    }
    ToOutFloat(0,hasBeenAlive);
    smoothingIterator++;
}

void vinylcontrol::m_signal(int nframes, float *const *in, float *const *out)
{

        int i;
        int n;
        // convert to pcm
        for(n = 0; n < nframes; n++)
            for(i = 0; i < DEVICE_CHANNELS; i++)
                pcm[n * DEVICE_CHANNELS + i] = (signed short)(SCALE*(float)in[i][n]);

	timecoder_submit(&tc,pcm,n);

        m_output_values();

}  // end m_signal

