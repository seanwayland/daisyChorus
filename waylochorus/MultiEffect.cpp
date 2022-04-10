#include "daisysp.h"
#include "daisy_pod.h"
#include <vector>

// Set max delay time to 0.75 of samplerate.
#define MAX_DELAY static_cast<size_t>(48000 * 2.5f)
#define CHRDEL 0
#define DEL 1
#define COR 2
#define PHR 3
#define OCT 4
#define WAYLOCHORUS 5
#define SR 48000
#define M_PI   3.14159265358979323846  /* pi */


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

int mCircularBufferWriteHead =0;
int mCircularBufferWriteHeadTwo =0;
int mCircularBufferWriteHeadThree =0;
int mCircularBufferWriteHeadFour = 0;
int mDelayReadHead = 0;
int mDelayTwoReadHead = 0;
int mDelayThreeReadHead = 0;
int mDelayFourReadHead =0;

float mRateParameter = 1.0;


float mLFOphase = 0;
float mLFOphaseTwo= 0;
float mLFOphaseThree= 0;
float mLFOphaseFour= 0;

//float *mCircularBufferLeft= new float[SR];
std::vector <float> mCircularBufferLeft (SR);

float *mCircularBufferRight= new float[SR];
float *mCircularBufferLeftTwo =new float[SR];
float *mCircularBufferRightTwo = new float[SR];
float *mCircularBufferLeftThree =new float[SR];
float *mCircularBufferRightThree= new float[SR];
float *mCircularBufferLeftFour = new float[SR];
float *mCircularBufferRightFour =new float[SR];





float  drywet;


//Helper functions
void Controls();

void GetReverbSample(float &outl, float &outr, float inl, float inr);

void GetDelaySample(float &outl, float &outr, float inl, float inr);

void GetChorusSample(float &outl, float &outr, float inl, float inr);

void GetPhaserSample(float &outl, float &outr, float inl, float inr);

void GetOctaveSample(float &outl, float &outr, float inl, float inr);

void GetWayloChorusSample(float &outl, float &outr, float inl, float inr);


float linInterp(float sample_x, float sample_x1, float inPhase)
{
    return (1 - inPhase) * sample_x + inPhase * sample_x1;
    
}


void AudioCallback(AudioHandle::InterleavingInputBuffer  in,
                   AudioHandle::InterleavingOutputBuffer out,
                   size_t                                size)
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
            case WAYLOCHORUS: GetWayloChorusSample(outl, outr, inl, inr); break;\
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

    for (int i = 0; i < SR -1; i++) {

        mCircularBufferLeft[i]= 0.0f;
        mCircularBufferRight[i]= 0.0f;
        mCircularBufferLeftTwo[i]= 0.0f;
        mCircularBufferRightTwo[i]= 0.0f;
        mCircularBufferLeftThree[i]= 0.0f;
        mCircularBufferRightThree[i]= 0.0f;
        mCircularBufferLeftFour[i]= 0.0f;
        mCircularBufferRightFour[i]= 0.0f;

}


    

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
            feedback    = k1*0.2 + k2*0.1; 
            /**
            crs.SetLfoDepth(0.15f+k2*0.35f);
            crs2.SetLfoDepth(0.2f+k2*0.26f);
            crs3.SetLfoDepth(0.23f+k2*0.37f);
            crs4.SetLfoDepth(0.25f+k2*0.29f);
            **/
            crs.SetLfoDepth(4.0f + k1*1.1f );
            crs2.SetLfoDepth(5.0f + k1*1.2f);
            crs3.SetLfoDepth(6.0f + k1*0.9f);
            crs4.SetLfoDepth(7.0f + k1*0.8f);
            /**Set lfo frequency Frequency in Hz */
            crs.SetLfoFreq(k2*0.6f);
            crs2.SetLfoFreq(k2*0.7f);
            crs3.SetLfoFreq(k2*0.8f); 
            crs4.SetLfoFreq(k2*0.9f); 
            
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
            crs.SetLfoDepth(4.0f + k1*1.1f );
            crs2.SetLfoDepth(5.0f + k1*1.2f);
            crs3.SetLfoDepth(6.0f + k1*0.9f);
            crs4.SetLfoDepth(7.0f + k1*0.8f);
            /**Set lfo frequency Frequency in Hz */
            crs.SetLfoFreq(k2*0.6f);
            crs2.SetLfoFreq(k2*0.7f);
            crs3.SetLfoFreq(k2*0.8f); 
            crs4.SetLfoFreq(k2*0.9f); 

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

        case WAYLOCHORUS:
            drywet = k1;
            mRateParameter = k2;
            break;
           
           
    }
}

void UpdateEncoder()
{
    mode = mode + pod.encoder.Increment();
    mode = (mode % 6 + 6) % 6;
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
    
    
    outl = crs.GetLeft() * drywet *2.0f  + crs3.GetLeft() * drywet*2.0f+  inl * (0.5f - drywet) + (feedback * outl*drywet*0.6f) + ((1.0f - feedback) * inl*drywet);;
    outr =  crs2.GetRight() * drywet*2.0f + + crs4.GetRight() * drywet*2.0f + inr * (0.5f - drywet) + (feedback * outr*drywet*0.6f) + ((1.0f - feedback) * inr*drywet);

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
    
    
    outl = crs.GetLeft() * drywet *2.5  + crs3.GetLeft() * drywet*2.5 +  inl * (0.5f - drywet);
    outr =  crs2.GetRight() * drywet*2.5 + + crs4.GetRight() * drywet*2.5 + inr * (0.5f - drywet);
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


void GetWayloChorusSample(float &outl, float &outr, float inl, float inr){

        float mDelayOneModRateParameter = 0.35f;
        float mDelayOneModRateParameterTwo = 0.2f;
        float mDelayOneModRateParameterThree = 0.4f;
        float mDelayOneModRateParameterFour = 0.5f;


        mLFOphase += mDelayOneModRateParameter * mRateParameter / SR;

        if ( mLFOphase > 1){
            mLFOphase -= 1;
        }

        float lfoOut = sin(2*M_PI * mLFOphase);

        
        mLFOphaseTwo += mDelayOneModRateParameterTwo * mRateParameter / SR;

        if ( mLFOphaseTwo > 1){
            mLFOphaseTwo -= 1;
        }

        float lfoOutTwo = sin(2*M_PI * mLFOphaseTwo);

        mLFOphaseThree += mDelayOneModRateParameterThree * mRateParameter / SR;

        if ( mLFOphaseThree > 1){
            mLFOphaseThree -= 1;
        }

        float lfoOutThree = sin(2*M_PI * mLFOphaseThree);

        
        mLFOphaseFour += mDelayOneModRateParameterFour * mRateParameter / SR;

        if ( mLFOphaseFour > 1){
            mLFOphaseFour -= 1;
        }

        float lfoOutFour = sin(2*M_PI * mLFOphaseFour);

        // map values from -1, 1  to .001 , .1 
        float lfoOutMapped = 0.001f + ((0.099f) * (lfoOut +1.f)) / (2.f);
        float lfoOutMappedTwo = 0.001f + ((0.099f) * (lfoOutTwo +1.f)) / (2.f);
        float lfoOutMappedThree = 0.001f + ((0.099f) * (lfoOutThree +1.f)) / (2.f);
        float lfoOutMappedFour = 0.001f + ((0.099f) * (lfoOutFour +1.f)) / (2.f);


        float mDelayOneTimeParameter = 0.0236f;
        float mDelayTwoTimeParameter = 0.030f;
        float mDelayThreeTimeParameter = 0.0361f;
        float mDelayFourTimeParameter = 0.0476f;

        int dtime = static_cast<int>(mDelayOneTimeParameter*SR);
        int dtimeTwo = static_cast<int>(mDelayTwoTimeParameter*SR);
        int dtimeThree = static_cast<int>(mDelayThreeTimeParameter*SR);
        int dtimeFour = static_cast<int>(mDelayFourTimeParameter*SR);

        int mDelayTimeInSamples = dtime*(1+ lfoOutMapped) ;
        int mDelayTwoTimeInSamples = dtimeTwo*(1+ lfoOutMappedTwo);
        int mDelayThreeTimeInSamples = dtimeThree*(1+ lfoOutMappedThree);
        int mDelayFourTimeInSamples = dtimeFour*(1+ lfoOutMappedFour) ;


        //// shove some of the input into the circular buffer also add some of the feedback
        mCircularBufferLeft[mCircularBufferWriteHead] = inl;
        mCircularBufferRight[mCircularBufferWriteHead] = inr;

        mCircularBufferLeftTwo[mCircularBufferWriteHead] = inl;
        mCircularBufferRightTwo[mCircularBufferWriteHead] = inr;

        mCircularBufferLeftThree[mCircularBufferWriteHead] = inl;
        mCircularBufferRightThree[mCircularBufferWriteHead] = inr;

        mCircularBufferLeftFour[mCircularBufferWriteHead] = inl;
        mCircularBufferRightFour[mCircularBufferWriteHead] = inr;


                // move the read head on the circular to the new delay position
        mDelayReadHead = mCircularBufferWriteHead - mDelayTimeInSamples;
        mDelayTwoReadHead = mCircularBufferWriteHeadTwo - mDelayTwoTimeInSamples;
        mDelayThreeReadHead = mCircularBufferWriteHeadThree - mDelayThreeTimeInSamples;
        mDelayFourReadHead = mCircularBufferWriteHeadFour - mDelayFourTimeInSamples;

                // if read head is below zero wrap around
        if (mDelayReadHead < 0){
            mDelayReadHead += SR;
        }
        
        if (mDelayTwoReadHead < 0){
            mDelayTwoReadHead += SR;
        }
        if (mDelayThreeReadHead < 0){
            mDelayThreeReadHead += SR;
        }
        if (mDelayFourReadHead < 0){
            mDelayFourReadHead += SR;
        }

        // get the integer part of the read head
        int readHeadX = (int)mDelayReadHead;
        // get the part of the readHead after the decimal point
        float readHeadFloat = mDelayReadHead - readHeadX;
        // next integer sample position
        int readHeadX1 = readHeadX + 1;
        
        // get the integer part of the read head
        int readHeadXTwo = (int)mDelayTwoReadHead;
        // get the part of the readHead after the decimal point
        float readHeadFloatTwo = mDelayTwoReadHead - readHeadXTwo;
        // next integer sample position
        int readHeadX1Two = readHeadXTwo + 1;
        
        // get the integer part of the read head
        int readHeadXThree = (int)mDelayThreeReadHead;
        // get the part of the readHead after the decimal point
        float readHeadFloatThree = mDelayThreeReadHead - readHeadXThree;
        // next integer sample position
        int readHeadX1Three = readHeadXThree + 1;
        
        // get the integer part of the read head
        int readHeadXFour = (int)mDelayFourReadHead;
        // get the part of the readHead after the decimal point
        float readHeadFloatFour = mDelayFourReadHead - readHeadXFour;
        // next integer sample position
        int readHeadX1Four = readHeadXFour + 1;




                // get the interpolated value of the delayed sample from the circular buffer
        float delay_sample_Left = linInterp(mCircularBufferLeft[readHeadX], mCircularBufferLeft[readHeadX1], readHeadFloat);
        float delay_sample_Right = linInterp(mCircularBufferRight[readHeadX], mCircularBufferRight[readHeadX1], readHeadFloat);
        float delay_sample_LeftTwo = linInterp(mCircularBufferLeftTwo[readHeadXTwo], mCircularBufferLeftTwo[readHeadX1Two], readHeadFloatTwo);
        float delay_sample_RightTwo = linInterp(mCircularBufferRightTwo[readHeadXTwo], mCircularBufferRightTwo[readHeadX1Two], readHeadFloatTwo);
        float delay_sample_LeftThree = linInterp(mCircularBufferLeftThree[readHeadXThree], mCircularBufferLeftThree[readHeadX1Three], readHeadFloatThree);
        float delay_sample_RightThree = linInterp(mCircularBufferRightThree[readHeadXThree], mCircularBufferRightThree[readHeadX1Three], readHeadFloatThree);
        float delay_sample_LeftFour = linInterp(mCircularBufferLeftFour[readHeadXFour], mCircularBufferLeftFour[readHeadX1Four], readHeadFloatFour);
        float delay_sample_RightFour = linInterp(mCircularBufferRightFour[readHeadXFour], mCircularBufferRightFour[readHeadX1Four], readHeadFloatFour);

        float mDelayOneGainParameter = 0.2f;
        float mDelayTwoGainParameter = 0.4f;
        float mDelayThreeGainParameter = 0.4f;
        float mDelayFourGainParameter = 0.4f;

        outl = inl*0.4f + delay_sample_Left*0.4f

                         
                         /***
                          + delay_sample_Left* *mDelayOneGainParameter* *drywet + delay_sample_Right* *mDelayOneGainParameter* *drywet
                          +delay_sample_LeftThree* *mDelayThreeGainParameter* *drywet+ delay_sample_RightThree* *mDelayThreeGainParameter* *drywet
                          +delay_sample_LeftFive* *mDelayFiveGainParameter* *drywet+ delay_sample_RightFive* *mDelayFiveGainParameter* *drywet
                          +delay_sample_LeftSeven* *mDelaySevenGainParameter* *drywet+ delay_sample_RightSeven* *mDelaySevenGainParameter* *drywet
                          ***/
                         
                       //  + delay_sample_Left*mDelayOneGainParameter + delay_sample_Right*mDelayOneGainParameter
                        // +delay_sample_LeftThree*mDelayThreeGainParameter+ delay_sample_RightThree*mDelayThreeGainParameter*drywet
                    
                        // + delay_sample_LeftTwo*mDelayTwoGainParameter*drywet+ delay_sample_RightTwo*mDelayTwoGainParameter*drywet
                        // + delay_sample_LeftFour*mDelayFourGainParameter*drywet+ delay_sample_RightFour*mDelayFourGainParameter*drywet

                         
                         ;
        
        outr = (inr*drywet
                         
                         
                         
                         /***
                          + delay_sample_LeftTwo* *mDelayTwoGainParameter* *drywet+ delay_sample_RightTwo* *mDelayTwoGainParameter* *drywet
                          + delay_sample_LeftFour* *mDelayFourGainParameter* *drywet+ delay_sample_RightFour* *mDelayFourGainParameter* *drywet
                          + delay_sample_LeftSix* *mDelaySixGainParameter* *drywet+ delay_sample_RightSix* *mDelaySixGainParameter* *drywet
                          + delay_sample_LeftEight* *mDelayEightGainParameter* *drywet+ delay_sample_RightEight* *mDelayEightGainParameter* *drywet
                          ***/
                         
                         + delay_sample_Left*mDelayOneGainParameter* drywet + delay_sample_Right*mDelayOneGainParameter*drywet
                         +delay_sample_LeftThree*mDelayThreeGainParameter* drywet+ delay_sample_RightThree*mDelayThreeGainParameter*drywet
                         + delay_sample_LeftTwo*mDelayTwoGainParameter* drywet+ delay_sample_RightTwo*mDelayTwoGainParameter*drywet
                         + delay_sample_LeftFour*mDelayFourGainParameter* drywet+ delay_sample_RightFour*mDelayFourGainParameter*drywet

                         
                         
                         );
        


        //outl = inl * (1.f - lfoOutMapped*10);
        outr =  inr * (1.f - lfoOutMappedTwo*10);

                //         // increment the buffer write head
        mCircularBufferWriteHead ++;
        mCircularBufferWriteHeadTwo ++;
        mCircularBufferWriteHeadThree ++;
        mCircularBufferWriteHeadFour ++;

        
        // wrap around if needed
        if (mCircularBufferWriteHead == SR-1){
            mCircularBufferWriteHead = 0;
        }
        if (mCircularBufferWriteHeadTwo == SR){
            mCircularBufferWriteHeadTwo = 0;
        }
        if (mCircularBufferWriteHeadThree == SR){
            mCircularBufferWriteHeadThree = 0;
        }
        if (mCircularBufferWriteHeadFour == SR){
            mCircularBufferWriteHeadFour = 0;
        }

    
            //outl =  psr.Process(inl * drywet) + inl*(1.0f-drywet);
           // outr =  psr.Process(inr * drywet) + inr*((1.0f-drywet));
            
}


