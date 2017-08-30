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
{
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

//==============================================================================
void ReaktorHostProcessor::prepareToPlay (double newSampleRate, int samplesPerBlock)
{
    if (wrappedInstance != nullptr)
        wrappedInstance->prepareToPlay(newSampleRate, samplesPerBlock);
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
    if (wrappedInstance != nullptr)
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
void ReaktorHostProcessor::getStateInformation (MemoryBlock& destData)
{
    if (wrappedInstance != nullptr)
    {
//        wrappedInstance->getStateInformation(destData);
        ScopedPointer<XmlElement> wrappedInstanceXml = wrappedInstance->getPluginDescription().createXml();
        copyXmlToBinary (*wrappedInstanceXml, destData);
    }
    else
    {
        XmlElement xml ("MYPLUGINSETTINGS");
        
        xml.setAttribute ("uiWidth", lastUIWidth);
        xml.setAttribute ("uiHeight", lastUIHeight);
        
        // Store the values of all our parameters, using their param ID as the XML attribute
        for (auto* param : getParameters())
            if (auto* p = dynamic_cast<AudioProcessorParameterWithID*> (param))
                xml.setAttribute (p->paramID, p->getValue());
        
        // then use this helper function to stuff it into the binary blob and return it..
        copyXmlToBinary (xml, destData);
    }
    
//    XmlElement xml ("MYPLUGINSETTINGS");
//    
//    xml.setAttribute ("uiWidth", lastUIWidth);
//    xml.setAttribute ("uiHeight", lastUIHeight);
//    
//    // Store the values of all our parameters, using their param ID as the XML attribute
//    for (auto* param : getParameters())
//        if (auto* p = dynamic_cast<AudioProcessorParameterWithID*> (param))
//            xml.setAttribute (p->paramID, p->getValue());
//    
//    if (wrappedInstance != nullptr)
//    {
//        copyXmlToBinary (*wrappedInstance->getPluginDescription().createXml(), destData);
//    }
//    
//    // then use this helper function to stuff it into the binary blob and return it..
//    copyXmlToBinary (xml, destData);
}

void ReaktorHostProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (wrappedInstance != nullptr)
    {
        wrappedInstance->setStateInformation (data, sizeInBytes);
    }
    else
    {
        // You should use this method to restore your parameters from this memory block,
        // whose contents will have been created by the getStateInformation() call.
        
        // This getXmlFromBinary() helper function retrieves our XML from the binary blob..
        ScopedPointer<XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
        
        if (xmlState != nullptr)
        {
            // make sure that it's actually our type of XML object..
            if (xmlState->hasTagName ("MYPLUGINSETTINGS"))
            {
                // ok, now pull out our last window size..
                lastUIWidth  = jmax (xmlState->getIntAttribute ("uiWidth", lastUIWidth), 400);
                lastUIHeight = jmax (xmlState->getIntAttribute ("uiHeight", lastUIHeight), 200);
                
                // Now reload our parameters..
                for (auto* param : getParameters())
                    if (auto* p = dynamic_cast<AudioProcessorParameterWithID*> (param))
                        p->setValue ((float) xmlState->getDoubleAttribute (p->paramID, p->getValue()));
            }
        }
    }
    
    
//    // This getXmlFromBinary() helper function retrieves our XML from the binary blob..
//    ScopedPointer<XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
//    
//    if (xmlState != nullptr)
//    {
//        // make sure that it's actually our type of XML object..
//        if (xmlState->hasTagName ("MYPLUGINSETTINGS"))
//        {
//            // ok, now pull out our last window size..
//            lastUIWidth  = jmax (xmlState->getIntAttribute ("uiWidth", lastUIWidth), 400);
//            lastUIHeight = jmax (xmlState->getIntAttribute ("uiHeight", lastUIHeight), 200);
//            
//            // Now reload our parameters..
//            for (auto* param : getParameters())
//                if (auto* p = dynamic_cast<AudioProcessorParameterWithID*> (param))
//                    p->setValue ((float) xmlState->getDoubleAttribute (p->paramID, p->getValue()));
//        }
//    }
}

//==============================================================================
// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ReaktorHostProcessor();
}
