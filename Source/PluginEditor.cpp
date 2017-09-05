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
    , hasEditor(false)
{
    
    addAndMakeVisible (openButton = new TextButton("open .fxp"));
    openButton->addListener(this);
    addAndMakeVisible(wrappedEditorComponent = new Component());
    
    // set resize limits for this plug-in
    setResizeLimits (400, 200, 1600, 800);

    // set our component's initial size to be the last one that was stored in the filter's settings
    setSize (owner.lastUIWidth, owner.lastUIHeight);
    
    //set button position and size
    openButton->setBounds(0, 0, owner.lastUIWidth, 20);
    
    formatManager.addDefaultFormats();
    
    startTimerHz (30);
    
    setVisible (true);
}

ReaktorHostProcessorEditor::~ReaktorHostProcessorEditor()
{
}

//==============================================================================
void ReaktorHostProcessorEditor::buttonClicked (Button* b)
{
    if (b == openButton)
        getProcessor().openFxpFile();
}

//==============================================================================
void ReaktorHostProcessorEditor::timerCallback()
{
    if (!hasEditor)
        if (AudioProcessorEditor* instanceEditor = getProcessor().getWrappedInstanceEditor())
        {
            Rectangle<int> wrappedInstanceEditorBounds (instanceEditor->getBounds());
            
            //set button position and size
            int buttonWidth = wrappedInstanceEditorBounds.getWidth();
            int buttonHeight = 20;
            openButton->setBounds(0, 0, buttonWidth, buttonHeight);

            //show wrapped instance editor in its full size
            wrappedEditorComponent->addAndMakeVisible(instanceEditor);
            wrappedEditorComponent->setBounds(0, buttonHeight, wrappedInstanceEditorBounds.getWidth(), wrappedInstanceEditorBounds.getHeight());
            
            //set whole size (potentially not necessary)
            setSize(buttonWidth, buttonHeight + wrappedInstanceEditorBounds.getHeight());
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
    hasEditor = false;
}

