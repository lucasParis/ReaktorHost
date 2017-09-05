/*
  ==============================================================================

   This file is part of the JUCE library.
   Copyright (c) 2017 - ROLI Ltd.

   JUCE is an open source library subject to commercial or open-source
   licensing.

   By using JUCE, you agree to the terms of both the JUCE 5 End-User License
   Agreement and JUCE 5 Privacy Policy (both updated and effective as of the
   27th April 2017).

   End User License Agreement: www.juce.com/juce-5-licence
   Privacy Policy: www.juce.com/juce-5-privacy-policy

   Or: You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   JUCE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

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
    if (wrappedInstance != nullptr)
    {
        wrappedInstance->prepareToPlay(newSampleRate, samplesPerBlock);
        isWrappedInstanceReadyToPlay = true;
    }
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

void ReaktorHostProcessor::openFxpFile()
{
#if JUCE_PLUGINHOST_VST
    File f ("/Users/nicolai/Desktop/Untitled.fxp");
    MemoryBlock mb;
    f.loadFileAsData (mb);
    VSTPluginFormat::loadFromFXBFile (wrappedInstance, mb.getData(), mb.getSize());
    
//#elif JUCE_PLUGINHOST_AU
//    AUPluginFormat::loadFromFXBFile (wrappedInstance, mb.getData(), mb.getSize());
#endif
}


void ReaktorHostProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    ScopedPointer<XmlElement> mainXmlElement (getXmlFromBinary (data, sizeInBytes));
    if (mainXmlElement != nullptr)
    {
        if (mainXmlElement->hasTagName ("REAKTOR_HOST_SETTINGS"))
        {
            lastUIWidth  = jmax (mainXmlElement->getIntAttribute ("uiWidth", lastUIWidth), 400);
            lastUIHeight = jmax (mainXmlElement->getIntAttribute ("uiHeight", lastUIHeight), 200);
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
