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
    //make sure we can open the default plugin file types
    formatManager.addDefaultFormats();
    
    //instance number
    addAndMakeVisible (instanceNbEditor = new TextEditor());
    instanceNbEditor->setText(String(getProcessor().getInstanceNumber()));
    instanceNbEditor->addListener(this);
    addAndMakeVisible(instanceNbLabel = new Label());
    instanceNbLabel->setText("instance nb:", dontSendNotification);
    
    //OSC STUFF
    // specify here on which UDP port number to receive incoming OSC messages
    int oscPort = owner.getOscPort();
    if (! connect (oscPort))
    {
        showConnectionErrorMessage ("Error: could not connect to UDP port " + String(oscPort));
    }
    // tell the component to listen for OSC messages matching this address:
    addListener (this); ///load patchnumero1
    
    
    
    
    //osc text editor
    addAndMakeVisible(oscPortEditor = new TextEditor());
    oscPortEditor->setText(String(oscPort));
    oscPortEditor->addListener(this);
    addAndMakeVisible(oscPortLabel = new Label());
    oscPortLabel->setText("osc port:", dontSendNotification);
    
    updateBounds(getProcessor().lastUIWidth, getProcessor().lastUIHeight);
    
    //wrapped instance component
    addAndMakeVisible(wrappedEditorComponent = new Component());
    
    //size and size limits for whole thing
    setResizeLimits (400, 200, 3200, 1600);
    setSize (owner.lastUIWidth, owner.lastUIHeight);
    
    //start timer and make ourselves visible
    startTimerHz (30);
    setVisible (true);
}

ReaktorHostProcessorEditor::~ReaktorHostProcessorEditor()
{
}

//==============================================================================
void ReaktorHostProcessorEditor::textEditorReturnKeyPressed(TextEditor& textEditor)
{
    if (&textEditor == oscPortEditor)
    {
        int oscPort = textEditor.getText().getIntValue();
        getProcessor().setOscPort(oscPort);
        if (! connect (oscPort))
        {
            showConnectionErrorMessage ("Error: could not connect to UDP port " + String(oscPort));
        }
    }
    else if (&textEditor == instanceNbEditor)
    {
        int instanceNb = textEditor.getText().getIntValue();
        getProcessor().setInstanceNumber(instanceNb);
    }
    unfocusAllComponents();
}

void ReaktorHostProcessorEditor::updateBounds(int width, int height)
{
    instanceNbLabel->   setBounds(0*width/4, 0, width/4, height);
    instanceNbEditor->  setBounds(1*width/4, 0, width/4, height);
    
    oscPortLabel->      setBounds(2*width/4, 0, width/4, height);
    oscPortEditor->     setBounds(3*width/4, 0, width/4, height);
}

//==============================================================================
void ReaktorHostProcessorEditor::timerCallback()
{
    if (!hasEditor)
        if (AudioProcessorEditor* instanceEditor = getProcessor().getWrappedInstanceEditor())
        {
            Rectangle<int> wrappedInstanceEditorBounds (instanceEditor->getBounds());
            
            //set button position and size
            int width = wrappedInstanceEditorBounds.getWidth();
            int buttonHeight = 20;
            updateBounds(width, buttonHeight);
            //show wrapped instance editor in its full size
            wrappedEditorComponent->addAndMakeVisible(instanceEditor);
            wrappedEditorComponent->setBounds(0, buttonHeight, wrappedInstanceEditorBounds.getWidth(), wrappedInstanceEditorBounds.getHeight());
            
            //set whole size (potentially not necessary)
            setSize(width, buttonHeight + wrappedInstanceEditorBounds.getHeight());
            hasEditor = true;
            stopTimer();
        }
}


void ReaktorHostProcessorEditor::paint (Graphics& g)
{


    g.setColour (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));
    g.fillAll();

}

void ReaktorHostProcessorEditor::resized()
{
    int width = getWidth();
    getProcessor().lastUIWidth = width;
    getProcessor().lastUIHeight = getHeight();
    
    updateBounds(width, 20);
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

void ReaktorHostProcessorEditor::oscMessageReceived (const OSCMessage& message)
{
    std::cout << "got osc" << std::endl;
    if (message.getAddressPattern().matches("/module/0/load"))
        if (message.size() == 1 && message[0].isString())
            getProcessor().loadFxpFile(message[0].getString());
            
}

void ReaktorHostProcessorEditor::oscBundleReceived (const OSCBundle & bundle)
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
                    getProcessor().loadFxpFile(message[0].getString());
                    getProcessor().oscOut.send ("/enable", (String) message[0].getString(), (int) getProcessor().getInstanceNumber());

                }
                
                
            }
            else if (message.getAddressPattern().toString().substring(0, 10).compare("/module/0/") == 0)
            {
//                std::cout << "modules 11111" << std::endl;
                String parameterName = message.getAddressPattern().toString().substring(9);
//                std::cout << "modules 11111 " << parameterName <<  std::endl;
                if(message[0].isFloat32())
                {
                    getProcessor().setVstCtrl(parameterName, message[0].getFloat32());
                }
                else if(message[0].isInt32())
                {
                    getProcessor().setVstCtrl(parameterName, (float)message[0].getInt32());
                }
                    
            }
            else if (message.getAddressPattern().toString().substring(0, 14).compare("/mixer/module/") == 0)
            {
//                std::cout << "modulessss 2222" << std::endl;
            }
        }

    }

    
    
}


void ReaktorHostProcessorEditor::showConnectionErrorMessage (const String& messageText)
{
    AlertWindow::showMessageBoxAsync (AlertWindow::WarningIcon, "Connection error", messageText, "OK");
}

