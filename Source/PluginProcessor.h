/*
  ==============================================================================

 Copyright (C) 2017  Lucas Paris
 
 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.

  ==============================================================================
*/

#pragma once

#include "../JuceLibraryCode/JuceHeader.h"

static String FXP_FOLDER_PATH = "/Users/lucas/Work/MOI/17_01_antiVolume/08_jucePatches/";

class ReaktorHostProcessor  : public AudioProcessor
                            , public OSCReceiver
                            , public OSCReceiver::Listener<OSCReceiver::MessageLoopCallback>
{
public:
    //==============================================================================
    ReaktorHostProcessor();
    ~ReaktorHostProcessor();
    
    void oscMessageReceived (const OSCMessage& message) override;
    void oscBundleReceived (const OSCBundle & bundle) override;


    //==============================================================================
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;
    
    void loadFxpFile(String fileName);
    
    void setVstCtrl(String name, float value)
    {
        //optimize: when loading fxp look for /fader/[0-9] and write into table for direct access
    #if JUCE_PLUGINHOST_VST
        for(int i = 0; i < wrappedInstance->getNumParameters(); i ++)
        {
//            std::cout << wrappedInstance->getParameterName(i) << std::endl;
            if(wrappedInstance->getParameterName(i).compare(name) == 0)
            {
                wrappedInstance->setParameter (i, value);
                break;
            }

        }
    #endif

    }
    

    //==============================================================================
    void processBlock (AudioBuffer<float>& buffer, MidiBuffer& midiMessages) override
    {
        process (buffer, midiMessages);
    }

    void processBlock (AudioBuffer<double>& buffer, MidiBuffer& midiMessages) override
    {
        process (buffer, midiMessages);
    }

    //==============================================================================
    bool hasEditor() const override                                             { return true; }
    AudioProcessorEditor* createEditor() override;
    AudioProcessorEditor* getWrappedInstanceEditor() const;

    //==============================================================================
    const String getName() const override                                       { return JucePlugin_Name; }

    bool acceptsMidi() const override                                           { return true; }
    bool producesMidi() const override                                          { return true; }

    double getTailLengthSeconds() const override                                { return 0.0; }

    //==============================================================================
    int getNumPrograms() override                                               { return 0; }
    int getCurrentProgram() override                                            { return 0; }
    void setCurrentProgram (int /*index*/) override                             {}
    const String getProgramName (int /*index*/) override                        { return String(); }
    void changeProgramName (int /*index*/, const String& /*name*/) override     {}
    
    int getOscPort()            {return oscPort;}
    void setOscPort(int port)   {oscPort = port;}
    
    int getInstanceNumber()             {return instanceNumber;}
    void setInstanceNumber(int number)  {instanceNumber = number;}

    //==============================================================================
    void getStateInformation (MemoryBlock&) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // these are used to persist the UI's size
    int lastUIWidth = 400, lastUIHeight = 200;
    
    void addFilterCallback (AudioPluginInstance* instance, const String& error, Point<int> pos);

    OSCSender oscOut;

private:
    //==============================================================================
    template <typename FloatType>
    void process (AudioBuffer<FloatType>& buffer, MidiBuffer& midiMessages);
    static BusesProperties getBusesProperties();
    
    ScopedPointer<AudioPluginInstance> wrappedInstance;
    ScopedPointer<AudioProcessorEditor> wrappedInstanceEditor;
    AudioPluginFormatManager formatManager;
    
    bool isWrappedInstanceReadyToPlay;
    int oscPort, instanceNumber;


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReaktorHostProcessor)
};
