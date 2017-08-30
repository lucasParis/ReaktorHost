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
#include "InternalFilters.h"
#include "GraphEditorPanel.h"


ReaktorHostProcessorEditor::ReaktorHostProcessorEditor (ReaktorHostProcessor& owner)
    : AudioProcessorEditor (owner)
{
    // set resize limits for this plug-in
    setResizeLimits (400, 200, 1600, 800);

    // set our component's initial size to be the last one that was stored in the filter's settings
    setSize (owner.lastUIWidth, owner.lastUIHeight);
    
    // initialise our settings file..
//    PropertiesFile::Options options;
//    options.applicationName     = "ModPlug";
//    options.filenameSuffix      = "settings";
//    options.osxLibrarySubFolder = "Preferences";
//    
//    appProperties = new ApplicationProperties();
//    appProperties->setStorageParameters (options);
    
    formatManager.addDefaultFormats();
//    
//    ScopedPointer<XmlElement> savedAudioState (appProperties->getUserSettings()->getXmlValue ("audioDeviceState"));
//    
//    String error = deviceManager.initialise (256, 256, savedAudioState, true);

//    addAndMakeVisible(graphDocumentComponent = new GraphDocumentComponent (formatManager, deviceManager), false);
    
//    restoreWindowStateFromString (getAppProperties().getUserSettings()->getValue ("mainWindowPos"));
    
//    InternalPluginFormat internalFormat;
//    internalFormat.getAllTypes (internalTypes);
    
//    ScopedPointer<XmlElement> savedPluginList (getAppProperties().getUserSettings()->getXmlValue ("pluginList"));
    
//    if (savedPluginList != nullptr)
//        knownPluginList.recreateFromXml (*savedPluginList);
//    
//    pluginSortMethod = (KnownPluginList::SortMethod) getAppProperties().getUserSettings()->getIntValue ("pluginSortMethod", KnownPluginList::sortByManufacturer);
//    
//    knownPluginList.addChangeListener (this);
    
//    if (auto* filterGraph = graphDocumentComponent->graph.get())
//        filterGraph->addChangeListener (this);
    
    startTimerHz (30);
    setVisible (true);
}

ReaktorHostProcessorEditor::~ReaktorHostProcessorEditor()
{
//    graphDocumentComponent = nullptr;
}

//==============================================================================
//==============================================================================
void ReaktorHostProcessorEditor::timerCallback()
{
    if (!hasEditor)
        if (AudioProcessorEditor* instanceEditor = getProcessor().getWrappedInstanceEditor())
        {
            addAndMakeVisible(instanceEditor);
            setBounds(instanceEditor->getBounds());
            hasEditor = true;
        }
}


void ReaktorHostProcessorEditor::paint (Graphics& g)
{
    g.setColour (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));
    g.fillAll();
}

void ReaktorHostProcessorEditor::resized()
{
//    Rectangle<int> r (getLocalBounds().reduced (8));
//    timecodeDisplayLabel.setBounds (r.removeFromTop (26));
//    midiKeyboard.setBounds (r.removeFromBottom (70));
//    r.removeFromTop (20);
//    Rectangle<int> sliderArea (r.removeFromTop (60));
//    gainSlider->setBounds (sliderArea.removeFromLeft (jmin (180, sliderArea.getWidth() / 2)));
//    delaySlider->setBounds (sliderArea.removeFromLeft (jmin (180, sliderArea.getWidth())));

    getProcessor().lastUIWidth = getWidth();
    getProcessor().lastUIHeight = getHeight();
}

bool ReaktorHostProcessorEditor::isInterestedInFileDrag (const StringArray&)
{
    return true;
}

void ReaktorHostProcessorEditor::fileDragEnter (const StringArray&, int, int)
{
}

void ReaktorHostProcessorEditor::fileDragMove (const StringArray&, int, int)
{
}

void ReaktorHostProcessorEditor::fileDragExit (const StringArray&)
{
}

void ReaktorHostProcessorEditor::filesDropped (const StringArray& files, int x, int y)
{
    //not handling replacing wrapped instance for now
    if (hasEditor)
        return;
    
    OwnedArray<PluginDescription> typesFound;
    knownPluginList.scanAndAddDragAndDroppedFiles (formatManager, files, typesFound);
    
    auto pos = getLocalPoint (this, Point<int> (x, y));
    
    for (int i = 0; i < jmin (5, typesFound.size()); ++i)
        if (auto* desc = typesFound.getUnchecked(i))
            createPlugin (*desc, pos);
}

void ReaktorHostProcessorEditor::createPlugin (const PluginDescription& desc, Point<int> p)
{
    struct AsyncCallback : public AudioPluginFormat::InstantiationCompletionCallback
    {
        AsyncCallback (ReaktorHostProcessor& g, Point<int> pos)  : owner (g), position (pos)
        {}
        
        void completionCallback (AudioPluginInstance* instance, const String& error) override
        {
            owner.addFilterCallback (instance, error, position);
        }
        
        ReaktorHostProcessor& owner;
        Point<int> position;
    };
    
    formatManager.createPluginInstanceAsync (desc, processor.getSampleRate(), getProcessor().getBlockSize(), new AsyncCallback (getProcessor(), p));
}

