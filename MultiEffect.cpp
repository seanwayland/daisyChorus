#include "daisysp.h"
#include "daisy_pod.h"

// Set max delay time to 0.75 of samplerate.
#define MAX_DELAY static_cast<size_t>(48000 * 2.5f)
#define CHRDEL 0
#define DEL 1
#define COR 2
#define PHR 3
#define OCT 4




using namespace daisysp;
using namespace daisy;

static DaisyPod pod;

/// declare effects used 
static Chorus                                    crs;
static Chorus                                    crs2;
static Chorus                                    crs3;
static Chorus                                    crs4;
static PitchShifter                              pst;

static Phaser                                    psr;
static Phaser                                    psr2;
static DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS dell;
static DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS delr;
static DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS dell2;
static DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS delr2;

static Parameter deltime;
int   mode = CHRDEL;
int   numstages;
uint32_t octDelSize;

float currentDelay, feedback, delayTarget, freq, freqtarget, lfotarget, lfo;


float  drywet;


//Helper functions
void Controls();

void GetReverbSample(float &outl, float &outr, float inl, float inr);

void GetDelaySample(float &outl, float &outr, float inl, float inr);

void GetChorusSample(float &outl, float &outr, float inl, float inr);

void GetPhaserSample(float &outl, float &outr, float inl, float inr);

void GetOctaveSample(float &outl, float &outr, float inl, float inr);


void AudioCallback(float *in, float *out, size_t size)
{
    float outl, outr, inl, inr;

    Controls();

    //audio
    for(size_t i = 0; i < size; i += 2)
    {
        inl = in[i];
        inr = in[i + 1];

        switch(mode)
        {
            case CHRDEL: GetReverbSample(outl, outr, inl, inr); break;
            case DEL: GetDelaySample(outl, outr, inl, inr); break;
            case COR: GetChorusSample(outl, outr, inl, inr); break;
            case PHR: GetPhaserSample(outl, outr, inl, inr); break;
            case OCT: GetOctaveSample(outl, outr, inl, inr); break;
            default: outl = outr = 0;
        }

        // left out
        out[i] = outl;

        // right out
        out[i + 1] = outr;
    }
}

int main(void)
{
    // initialize pod hardware and oscillator daisysp module
    float sample_rate;

    //Inits and sample rate
    pod.Init();
    sample_rate = pod.AudioSampleRate();
    // start effect processing

    dell.Init();
    delr.Init();
    dell2.Init();
    delr2.Init();
    crs.Init(sample_rate);
    crs2.Init(sample_rate);
    crs3.Init(sample_rate);
    crs4.Init(sample_rate);
    psr.Init(sample_rate);
    psr2.Init(sample_rate);
    pst.Init(sample_rate);


    //set parameters
    deltime.Init(pod.knob1, sample_rate * .05, MAX_DELAY, deltime.LOGARITHMIC);



    //delay parameters
    currentDelay = delayTarget = sample_rate * 0.75f;
    dell.SetDelay(currentDelay);
    delr.SetDelay(currentDelay);
    dell2.SetDelay(currentDelay+500);
    delr2.SetDelay(currentDelay+1000);



    //chorus parameters
    crs.SetFeedback(.1f);
    crs.SetDelay(.7f);
    crs2.SetFeedback(.1f);
    crs2.SetDelay(.82f);
    crs3.SetFeedback(.1f);
    crs3.SetDelay(.9f);
    crs4.SetFeedback(.1f);
    crs4.SetDelay(.97f);

    // phaser parameters
    numstages = 4;
    psr.SetPoles(numstages);
    psr2.SetPoles(numstages);
    freqtarget = freq = 0.f;
    lfotarget = lfo = 0.f;

    //pitchshifter parameters
    pst.SetTransposition(12.0f);
    /***
    sets delay size changing the timbre of the pitchshifting  ***/
    octDelSize = 256;
    pst.SetDelSize(octDelSize);


    // start callback
    pod.StartAdc();
    pod.StartAudio(AudioCallback);

    while(1) {}
}

void UpdateKnobs(float &k1, float &k2)
{
    k1 = pod.knob1.Process();
    k2 = pod.knob2.Process();

    switch(mode)
    {
        /// a allan holdsworth inspired chorus plus stereo delay lead sound 
        case CHRDEL:
            drywet = k1;
            delayTarget = deltime.Process();
            feedback    = k2;
            crs.SetLfoDepth(0.15f+k2*0.35f);
            crs2.SetLfoDepth(0.2f+k2*0.26f);
            crs3.SetLfoDepth(0.23f+k2*0.37f);
            crs4.SetLfoDepth(0.25f+k2*0.29f);
            /**Set lfo frequency Frequency in Hz */
            crs.SetLfoFreq(1.0f+k2*1.6f);
            crs2.SetLfoFreq(1.5f+k2*1.7f);
            crs3.SetLfoFreq(1.2f+ k2*2.3f);
            crs4.SetLfoFreq(1.4f + k2*2.4f);
            
            break;
            // a stereo delay without chorus 
        case DEL:
            delayTarget = deltime.Process();
            feedback    = k2;
            break;
            // a chorus with no delay/reverb
        case COR:
            drywet = k1;
            /** How much to modulate the delay by Works 0-1.    */
            crs.SetLfoDepth(0.15f+k2*0.35f);
            crs2.SetLfoDepth(0.2f+k2*0.26f);
            crs3.SetLfoDepth(0.23f+k2*0.37f);
            crs4.SetLfoDepth(0.25f+k2*0.29f);
            /**Set lfo frequency Frequency in Hz */
            crs.SetLfoFreq(1.0f+k2*1.6f);
            crs2.SetLfoFreq(1.5f+k2*1.7f);
            crs3.SetLfoFreq(1.2f+ k2*2.3f);
            crs4.SetLfoFreq(1.4f + k2*2.4f);
            break;
        case PHR:
            //ap_freq Frequency in Hz
            /*** = k1 * k1 * 7000;
            fonepole(freq, freqtarget, .0001f); //smooth at audio rate     
            psr.SetFreq(freq);
            //feedback Amount from 0-1.
            psr.SetFeedback(k2);
            //set lfo_freq Frequency in Hz
            lfo = k2*10.0f;
            fonepole(lfo, lfotarget, .0001f); //smooth at audio rate
            psr.SetLfoDepth(lfo);
            psr.SetLfoFreq(k2*10);
            
            // depth Works 0-1
            ***/

            drywet = k1;
            lfo = k2;

            break;
        case OCT:
            drywet = k1;
            break;



           
    }
}

void UpdateEncoder()
{
    mode = mode + pod.encoder.Increment();
    mode = (mode % 5 + 5) % 5;
}

void UpdateLeds(float k1, float k2)
{
    pod.led1.Set(
        k1 * (mode == 2), k1 * (mode == 1), k1 * (mode == 0 || mode == 3));
    pod.led2.Set(
        k2 * (mode == 3), k2 * (mode == 2 || mode == 4), k2 * (mode == 0 || mode == 4));

    pod.UpdateLeds();
}

void Controls()
{
    float k1, k2;
    delayTarget = feedback = drywet = 0;

    pod.ProcessAnalogControls();
    pod.ProcessDigitalControls();

    UpdateKnobs(k1, k2);

    UpdateEncoder();

    UpdateLeds(k1, k2);
}

// process audio and add delays and modulated delays in 
void GetReverbSample(float &outl, float &outr, float inl, float inr)
{

    fonepole(currentDelay, delayTarget, .00007f);
    delr.SetDelay(currentDelay);
    dell.SetDelay(currentDelay);
    delr2.SetDelay(20000.0f);
    dell2.SetDelay(30000.0f);
    outl = dell.Read() + dell2.Read();
    outr = delr.Read() + delr2.Read();
    dell.Write((feedback * outl) + inl);
    dell2.Write((feedback * outl) + inl);
    delr.Write((feedback * outr) + inr);
    delr2.Write((feedback * outr) + inr);


    crs.Process(inl);
    crs2.Process(inr);
    crs3.Process(inl);
    crs4.Process(inr);
    
    
    outl = crs.GetLeft() * drywet   + crs3.GetLeft() * drywet+  inl * (1.2f - drywet) + (feedback * outl) + ((1.0f - feedback) * inl);;
    outr =  crs2.GetRight() * drywet + + crs4.GetRight() * drywet + inr * (1.2f - drywet) + (feedback * outr) + ((1.0f - feedback) * inr);

}
// process audio adding in delays 
void GetDelaySample(float &outl, float &outr, float inl, float inr)
{
    fonepole(currentDelay, delayTarget, .00007f);
    delr.SetDelay(currentDelay);
    dell.SetDelay(currentDelay);
    delr2.SetDelay(20000.0f);
    dell2.SetDelay(30000.0f);
    outl = dell.Read() + dell2.Read();
    outr = delr.Read() + delr2.Read();

    dell.Write((feedback * outl) + inl);
    dell2.Write((feedback * outl) + inl);
    outl = (feedback * outl) + ((1.0f - feedback) * inl);

    delr.Write((feedback * outr) + inr);
    delr2.Write((feedback * outr) + inr);
    outr = (feedback * outr) + ((1.0f - feedback) * inr);
}


// process audio adding in chorus 
void GetChorusSample(float &outl, float &outr, float inl, float inr)
{

    crs.Process(inl);
    crs2.Process(inr);
    crs3.Process(inl);
    crs4.Process(inr);
    
    
    outl = crs.GetLeft() * drywet *1.5  + crs3.GetLeft() * drywet*1.5 +  inl * (1.5f - drywet);
    outr =  crs2.GetRight() * drywet*1.5 + + crs4.GetRight() * drywet*1.5 + inr * (1.5f - drywet);
}

void GetPhaserSample(float &outl, float &outr, float inl, float inr){
           
            freq = 7000.0f;
            fonepole(freq, freqtarget, .0001f); //smooth at audio rate
            psr.SetFreq(freq);
            fonepole(lfo, lfotarget, .0001f); //smooth at audio rate
            psr.SetLfoDepth(lfo);
            psr.SetFeedback(.2f);
            psr2.SetFeedback(.3f);
            psr.SetLfoFreq(30.0f);
            psr2.SetLfoFreq(40.0f);

            freq = 4000.0f;
            fonepole(freq, freqtarget, .0001f); //smooth at audio rate
            psr2.SetFreq(freq);
            fonepole(lfo, lfotarget, .0001f); //smooth at audio rate
            psr2.SetLfoDepth(lfo);
            crs.Process(inl);

            outl = crs.GetLeft() * drywet*0.5 + psr.Process(inl) * drywet + inl * (1.f - drywet);
            outr = crs.GetRight() * drywet*0.3 + psr2.Process(inr) * drywet + inr * (1.f - drywet);

    
            //outl =  psr.Process(inl * drywet) + inl*(1.0f-drywet);
           // outr =  psr.Process(inr * drywet) + inr*((1.0f-drywet));
            
}

            void GetOctaveSample(float &outl, float &outr, float inl, float inr){

            outl = crs.GetLeft() * drywet*0.2 + pst.Process(inl) * drywet + inl * (1.f - drywet);
            outr = crs.GetRight() * drywet*0.2 + pst.Process(inr) * drywet + inr * (1.f - drywet);


            }



