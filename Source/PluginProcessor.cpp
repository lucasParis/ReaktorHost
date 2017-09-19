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

#include "PluginProcessor.h"
#include "PluginEditor.h"

AudioProcessor* JUCE_CALLTYPE createPluginFilter();


ReaktorHostProcessor::ReaktorHostProcessor()
: AudioProcessor (getBusesProperties())
, wrappedInstance(nullptr)
, wrappedInstanceEditor (nullptr)
, isWrappedInstanceReadyToPlay(false)
, oscPort(1234)
, instanceNumber(1)
{
    formatManager.addDefaultFormats();
}

ReaktorHostProcessor::~ReaktorHostProcessor()
{
}


bool ReaktorHostProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (wrappedInstance != nullptr)
    {
        JUCE_COMPILER_WARNING("should probably check which bus layouts reaktor supports")
//        return wrappedInstance->isBusesLayoutSupported(layouts);
        return true;
    }
    else
    {
        // Only mono/stereo and input/output must have same layout
        const AudioChannelSet& mainOutput = layouts.getMainOutputChannelSet();
        const AudioChannelSet& mainInput  = layouts.getMainInputChannelSet();
        
        // input and output layout must either be the same or the input must be disabled altogether
        if (! mainInput.isDisabled() && mainInput != mainOutput)
            return false;
        
        // do not allow disabling the main buses
        if (mainOutput.isDisabled())
            return false;
        
        // only allow stereo and mono
        if (mainOutput.size() > 2)
            return false;
        
        return true;
    }
}

AudioProcessor::BusesProperties ReaktorHostProcessor::getBusesProperties()
{
    return BusesProperties().withInput  ("Input",  AudioChannelSet::stereo(), true)
                            .withOutput ("Output", AudioChannelSet::stereo(), true);
}

void ReaktorHostProcessor::addFilterCallback (AudioPluginInstance* instance, const String& error, Point<int> pos)
{
    if (instance == nullptr)
    {
        AlertWindow::showMessageBox (AlertWindow::WarningIcon, TRANS("Couldn't create filter"), error);
    }
    else
    {
        instance->prepareToPlay(getSampleRate(), getBlockSize());
        instance->enableAllBuses();
        
        wrappedInstance = nullptr;
        wrappedInstanceEditor = nullptr;
        wrappedInstance = instance;
        wrappedInstanceEditor = instance->createEditor();
        isWrappedInstanceReadyToPlay = true;
    }
}

//==============================================================================
void ReaktorHostProcessor::prepareToPlay (double newSampleRate, int samplesPerBlock)
{
    
    //OSC STUFF
    // specify here on which UDP port number to receive incoming OSC messages
    

    
    if (wrappedInstance != nullptr)
    {
        wrappedInstance->prepareToPlay(newSampleRate, samplesPerBlock);
        isWrappedInstanceReadyToPlay = true;
    }
    
    
    oscOutP5.connect ("127.0.0.1", 9000);
    oscOutMixer.connect ("127.0.0.1", 10000);

    int oscPort = getOscPort();
    if (! connect (oscPort))
    {
        //        (ReaktorHostProcessorEditor*)(getWrappedInstanceEditor())->showConnectionErrorMessage ("Error: could not connect to UDP port " + String(oscPort));
    }
    
    
    // tell the component to listen for OSC messages matching this address:
    //    addListener (this); ///load patchnumero1
    OSCReceiver::addListener(this);
    
    //OSCOUT
}

void ReaktorHostProcessor::releaseResources()
{
    if (wrappedInstance != nullptr)
        wrappedInstance->releaseResources();
}

void ReaktorHostProcessor::reset()
{
    if (wrappedInstance != nullptr)
        wrappedInstance->reset();
}

template <typename FloatType>
void ReaktorHostProcessor::process (AudioBuffer<FloatType>& buffer, MidiBuffer& midiMessages)
{
    if (wrappedInstance != nullptr && isWrappedInstanceReadyToPlay)
    {
        wrappedInstance->setPlayHead(getPlayHead());
        wrappedInstance->processBlock(buffer, midiMessages);
    }
    
     MidiBuffer::Iterator iterator (midiMessages);
    MidiMessage message;

    int sampleNumber;

    while (iterator.getNextEvent (message, sampleNumber))
    {
        if (message.isController())
        {
//            std::cout << "midi" << std::endl;
//            std::cout << message.getControllerNumber() << std::endl;
//            std::cout << message.getControllerValue() << std::endl;
            
            oscOutP5.send ("/ctrl", (int) message.getControllerNumber(), (int) message.getControllerValue(), (int) instanceNumber);
            
        }
    }
    //    midiMessages.clear();
    
}

//==============================================================================
AudioProcessorEditor* ReaktorHostProcessor::createEditor()
{
    return new ReaktorHostProcessorEditor (*this);
}

AudioProcessorEditor* ReaktorHostProcessor::getWrappedInstanceEditor() const
{
    return wrappedInstanceEditor;
}

//==============================================================================
static XmlElement* createBusLayoutXml (const AudioProcessor::BusesLayout& layout, const bool isInput)
{
    const Array<AudioChannelSet>& buses = (isInput ? layout.inputBuses : layout.outputBuses);
    
    XmlElement* xml = new XmlElement (isInput ? "INPUTS" : "OUTPUTS");
    
    const int n = buses.size();
    for (int busIdx = 0; busIdx < n; ++busIdx)
    {
        XmlElement* bus = new XmlElement ("BUS");
        bus->setAttribute ("index", busIdx);
        
        const AudioChannelSet& set = buses.getReference (busIdx);
        const String layoutName = set.isDisabled() ? "disabled" : set.getSpeakerArrangementAsString();
        
        bus->setAttribute ("layout", layoutName);
        
        xml->addChildElement (bus);
    }
    
    return xml;
}

static void readBusLayoutFromXml (AudioProcessor::BusesLayout& busesLayout, AudioProcessor* plugin, const XmlElement& xml, const bool isInput)
{
    Array<AudioChannelSet>& targetBuses = (isInput ? busesLayout.inputBuses : busesLayout.outputBuses);
    int maxNumBuses = 0;
    
    if (auto* buses = xml.getChildByName (isInput ? "INPUTS" : "OUTPUTS"))
    {
        forEachXmlChildElementWithTagName (*buses, e, "BUS")
        {
            const int busIdx = e->getIntAttribute ("index");
            maxNumBuses = jmax (maxNumBuses, busIdx + 1);
            
            // the number of buses on busesLayout may not be in sync with the plugin after adding buses
            // because adding an input bus could also add an output bus
            for (int actualIdx = plugin->getBusCount (isInput) - 1; actualIdx < busIdx; ++actualIdx)
                if (! plugin->addBus (isInput)) return;
            
            for (int actualIdx = targetBuses.size() - 1; actualIdx < busIdx; ++actualIdx)
                targetBuses.add (plugin->getChannelLayoutOfBus (isInput, busIdx));
            
            const String& layout = e->getStringAttribute("layout");
            
            if (layout.isNotEmpty())
                targetBuses.getReference (busIdx) = AudioChannelSet::fromAbbreviatedString (layout);
        }
    }
    
    // if the plugin has more buses than specified in the xml, then try to remove them!
    while (maxNumBuses < targetBuses.size())
    {
        if (! plugin->removeBus (isInput))
            return;
        
        targetBuses.removeLast();
    }
}

void ReaktorHostProcessor::getStateInformation (MemoryBlock& destData)
{
    XmlElement mainXmlElement ("REAKTOR_HOST_SETTINGS");
    
    mainXmlElement.setAttribute ("uiWidth", lastUIWidth);
    mainXmlElement.setAttribute ("uiHeight", lastUIHeight);
    mainXmlElement.setAttribute ("oscPort", oscPort);
    mainXmlElement.setAttribute ("instanceNumber", instanceNumber);
    
    if (wrappedInstance != nullptr){
        XmlElement* wrappedInstanceXmlElement = new XmlElement ("WRAPPED_INSTANCE");
        
        PluginDescription pd;
        wrappedInstance->fillInPluginDescription (pd);
        wrappedInstanceXmlElement->addChildElement (pd.createXml());
        
        XmlElement* state = new XmlElement ("WRAPPED_INSTANCE_STATE");
        
        MemoryBlock m;
        wrappedInstance->getStateInformation (m);
        state->addTextElement (m.toBase64Encoding());
        wrappedInstanceXmlElement->addChildElement (state);

        XmlElement* layouts = new XmlElement ("WRAPPED_INSTANCE_LAYOUT");
        const AudioProcessor::BusesLayout layout = wrappedInstance->getBusesLayout();
        
        const bool isInputChoices[] = { true, false };
        for (bool isInput : isInputChoices)
            layouts->addChildElement (createBusLayoutXml (layout, isInput));
        
        wrappedInstanceXmlElement->addChildElement (layouts);
        
        mainXmlElement.addChildElement(wrappedInstanceXmlElement);
    }
    copyXmlToBinary (mainXmlElement, destData);
}

void ReaktorHostProcessor::loadFxpFile(String fileName)
{
    File f (FXP_FOLDER_PATH + fileName + ".fxp");
    if (f.exists())
    {
        MemoryBlock mb;
        f.loadFileAsData (mb);
#if JUCE_PLUGINHOST_VST
        VSTPluginFormat::loadFromFXBFile (wrappedInstance, mb.getData(), mb.getSize());
//#elif JUCE_PLUGINHOST_AU
        //AUPluginFormat::loadFromFXBFile (wrappedInstance, mb.getData(), mb.getSize());
#endif
    }
}

void ReaktorHostProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    ScopedPointer<XmlElement> mainXmlElement (getXmlFromBinary (data, sizeInBytes));
    if (mainXmlElement != nullptr)
    {
        if (mainXmlElement->hasTagName ("REAKTOR_HOST_SETTINGS"))
        {
            lastUIWidth     = mainXmlElement->getIntAttribute ("uiWidth", lastUIWidth);
            lastUIHeight    = mainXmlElement->getIntAttribute ("uiHeight", lastUIHeight);
            oscPort         = mainXmlElement->getIntAttribute("oscPort", oscPort);
            instanceNumber  = mainXmlElement->getIntAttribute("instanceNumber", instanceNumber);
            
            oscOutP5.connect ("127.0.0.1", 9000);
            oscOutMixer.connect ("127.0.0.1", 10000);
            int oscPort = getOscPort();
            if (! connect (oscPort))
            {
                //        (ReaktorHostProcessorEditor*)(getWrappedInstanceEditor())->showConnectionErrorMessage ("Error: could not connect to UDP port " + String(oscPort));
            }
        }
        
        forEachXmlChildElementWithTagName (*mainXmlElement, wrappedInstanceXmlElement, "WRAPPED_INSTANCE")
        {
            PluginDescription pd;
            forEachXmlChildElement (*wrappedInstanceXmlElement, e)
            {
                if (pd.loadFromXml (*e))
                    break;
            }
            
            String errorMessage;
            wrappedInstance = formatManager.createPluginInstance (pd, getSampleRate(), getBlockSize(), errorMessage);
            
            if (wrappedInstance == nullptr)
                return;
            
            if (const XmlElement* const layoutEntity = wrappedInstanceXmlElement->getChildByName ("WRAPPED_INSTANCE_LAYOUT"))
            {
                AudioProcessor::BusesLayout layout = wrappedInstance->getBusesLayout();
                
                const bool isInputChoices[] = { true, false };
                for (bool isInput : isInputChoices)
                    readBusLayoutFromXml (layout, wrappedInstance, *layoutEntity, isInput);
                
                wrappedInstance->setBusesLayout (layout);
            }
            
            if (const XmlElement* const state = wrappedInstanceXmlElement->getChildByName ("WRAPPED_INSTANCE_STATE"))
            {
                MemoryBlock m;
                m.fromBase64Encoding (state->getAllSubText());
                wrappedInstance->setStateInformation (m.getData(), (int) m.getSize());
                wrappedInstance->prepareToPlay(44100, getBlockSize());
                wrappedInstanceEditor = wrappedInstance->createEditor();
                isWrappedInstanceReadyToPlay = true;
                return;
            }
        }
    }
}
//==============================================================================


// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ReaktorHostProcessor();
}


void ReaktorHostProcessor::oscMessageReceived (const OSCMessage& message)
{
    std::cout << "got osc" << std::endl;
    if (message.getAddressPattern().matches("/module/0/load"))
        if (message.size() == 1 && message[0].isString())
            loadFxpFile(message[0].getString());

}

void ReaktorHostProcessor::oscBundleReceived (const OSCBundle & bundle)
{
//    std::cout << "got osc" << std::endl;
    for(int i = 0; i < bundle.size(); i++)
    {
        if(bundle.operator[](i).isMessage())
        {
            const OSCMessage& message = bundle.operator[](i).getMessage();
            if (message.getAddressPattern().matches("/module/0/load"))
            {
                if (message.size() == 1 && message[0].isString())
                {
                    loadFxpFile(message[0].getString());
                    oscOutP5.send ("/enable", (String) message[0].getString(), (int) getInstanceNumber());

                }


            }
            else if (message.getAddressPattern().matches("/startTimer"))
            {
//                if (message.size() == 1 && message[0].isString())
//                {
//                    loadFxpFile(message[0].getString());
//                    oscOutP5.send ("/enable", (String) message[0].getString(), (int) getInstanceNumber());
//                    
//                }
                // send to other instance number 10.10.10.[2-4] port 8000
                
                
            }
            else if (message.getAddressPattern().toString().substring(0, 10).compare("/module/0/") == 0)
            {
//                std::cout << "modules 11111" << std::endl;
                String parameterName = message.getAddressPattern().toString().substring(9);
//                std::cout << "modules 11111 " << parameterName <<  std::endl;
                if(message[0].isFloat32())
                {
                    setVstCtrl(parameterName, message[0].getFloat32());
                }
                else if(message[0].isInt32())
                {
                    setVstCtrl(parameterName, (float)message[0].getInt32());
                }

            }
            else //if (message.getAddressPattern().toString().substring(0, 14).compare("/mixer/module/") == 0)
            {
                oscOutMixer.send(message);
                oscOutP5.send(message);
//                String addr = message.getAddressPattern().toString();
//
//                if(message[0].isFloat32())
//                {
//                    oscOutP5.send (addr, (float)message[0].getFloat32());
//
//                }
//                else if(message[0].isInt32())
//                {
//                    oscOutP5.send (addr, (int)message[0].getInt32());
//
//                }
            }
        }
    }



}


