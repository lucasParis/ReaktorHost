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
    setResizeLimits (400, 200, 3200, 1600);

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
    
    openButton->setBounds(0, 0, getProcessor().lastUIWidth, 20);
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

