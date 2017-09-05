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
#include "PluginProcessor.h"
#include "GraphEditorPanel.h"



class ReaktorHostProcessorEditor : public AudioProcessorEditor
                                 , private Timer
                                 , public FileDragAndDropTarget
                                 , private Button::Listener
{
public:
    ReaktorHostProcessorEditor (ReaktorHostProcessor&);
    ~ReaktorHostProcessorEditor();

    void buttonClicked (Button* b) override;
    
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
    
    ScopedPointer<TextButton> openButton;
    ScopedPointer<Component> wrappedEditorComponent;
    
    bool hasEditor;
};
