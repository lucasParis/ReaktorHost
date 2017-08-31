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

#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include "PluginProcessor.h"
#include "GraphEditorPanel.h"



class ReaktorHostProcessorEditor : public AudioProcessorEditor
                                 , private Timer
                                 , public FileDragAndDropTarget
{
public:
    ReaktorHostProcessorEditor (ReaktorHostProcessor&);
    ~ReaktorHostProcessorEditor();

    //==============================================================================
    void paint (Graphics&) override;
    void resized() override;
    void timerCallback() override;

    bool isInterestedInFileDrag (const StringArray&) override;
    void fileDragEnter (const StringArray&, int, int) override;
    void fileDragMove (const StringArray&, int, int) override;
    void fileDragExit (const StringArray&) override;
    void filesDropped (const StringArray& files, int x, int y) override;

private:

    ReaktorHostProcessor& getProcessor() const
    {
        return static_cast<ReaktorHostProcessor&> (processor);
    }
    
    void createPlugin (const PluginDescription& desc, Point<int> p);

    AudioPluginFormatManager formatManager;
    KnownPluginList knownPluginList;
    
    bool hasEditor;
};
