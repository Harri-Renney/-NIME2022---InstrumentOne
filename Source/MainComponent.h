#pragma once

#include <JuceHeader.h>


#include "SenselWrapper.h"
#include "Wavetable_Exciter.h"
#include "FDTD_Accelerated.hpp"

using namespace juce;

//==============================================================================
/*
    This component lives inside our window, and this is where you should put all
    your controls and content.
*/
class MainComponent  : public juce::AudioAppComponent,
	public  juce::Button::Listener,
	public  juce::Slider::Listener,
	public juce::HighResolutionTimer,
	public juce::ChangeListener
{
public:
    //==============================================================================
    MainComponent();
    ~MainComponent() override;

    //==============================================================================
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

	//Interface//
	void buttonClicked(Button* btn) override;
	void sliderValueChanged(Slider* sld) override;
	void sliderDragEnded(Slider* sld) override;
	void hiResTimerCallback() override;


    //==============================================================================
    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    //==============================================================================
    
	//Physical Model.
	const std::string physicalModelPath_ = "../../Source/use_case_001_512.json";
	FDTD_Accelerated* simulationModel;

	//OpenGL Render.
	uint32_t framerate = 1000;

	//Synchronisation.
	std::mutex mutexInit;
	

	//Wavetable Synthesizer//
	std::vector<float> wave = { 0.057564,0.114937,0.171929,0.228351,0.284015,0.338738,0.392337,0.444635,0.495459,0.544639,0.592013,0.637424,0.680721,0.721760,0.760406,0.796530,0.830012,0.860742,0.888617,0.913546,0.935444,0.954240,0.969872,0.982287,0.991445,0.997315,0.999877,0.999123,0.995055,0.987688,0.977045,0.963162,0.946085,0.925870,0.902585,0.876307,0.847122,0.815128,0.780430,0.743145,0.703395,0.661312,0.617036,0.570714,0.522499,0.472551,0.421036,0.368125,0.313993,0.258820,0.202788,0.146084,0.088895,0.031412,-0.026176,-0.083677,-0.140900,-0.197656,-0.253757,-0.309016,-0.363250,-0.416280,-0.467929,-0.518026,-0.566405,-0.612906,-0.657374,-0.699662,-0.739630,-0.777145,-0.812083,-0.844327,-0.873771,-0.900318,-0.923879,-0.944376,-0.961741,-0.975916,-0.986855,-0.994522,-0.998890,-0.999945,-0.997684,-0.992115,-0.983255,-0.971135,-0.955794,-0.937283,-0.915664,-0.891008,-0.863397,-0.832923,-0.799686,-0.763798,-0.725376,-0.684549,-0.641452,-0.596227,-0.549025,-0.500003,-0.449322,-0.397151,-0.343663,-0.289035,-0.233449,-0.177088,-0.120140,-0.062794,-0.005240 };
	WavetableExciter wavetableExciter_;
	Sensel senselInterface;
	float inputExcitation[10000];
	int exciteDuration = 1;
	bool isInit = false;

	//Interface//
	TextButton btnExcite;
	TextButton btnCreateDrum;
	TextButton btnPresetKick;
	TextButton btnPresetDrum;
	TextButton btnPresetCymbal;
	TextButton btnPresetHighHat;
	TextButton btnPresetWood;
	TextButton btnControlExcitation;
	TextButton btnControlListener;
	TextButton btnRectangle;
	TextButton btnCircle;
	TextButton btnTogglePixelated;
	Slider sldGridWidth, sldGridHeight;
	Label lblDrumOne;
	Slider sldPropagationOne;
	Slider sldDampingOne;
	Label lblDrumTwo;
	Slider sldPropagationTwo;
	Slider sldDampingTwo;
	Slider sldInputDuration;
	Label lblGridWidth;
	Label lblGridHeight;
	Label lblPropagationOne;
	Label lblDampingOne;
	Label lblPropagationTwo;
	Label lblDampingTwo;
	Label lblBoundary;
	Label lblInputDuration;
	Label lblPresets;
	Label lblControl;
	Label lblShape;
	Label lblFPS;
	Label lblLatency;

	//Control Audio Devices
	juce::Random random;
	juce::AudioDeviceSelectorComponent audioSetupComp;
	juce::Label cpuUsageLabel;
	juce::Label cpuUsageText;
	juce::TextEditor diagnosticsBox;

	void changeListenerCallback(juce::ChangeBroadcaster*) override
	{
		mutexInit.lock();
		dumpDeviceInfo();
		mutexInit.unlock();
	}

	static juce::String getListOfActiveBits(const juce::BigInteger& b)
	{
		juce::StringArray bits;

		for (auto i = 0; i <= b.getHighestBit(); ++i)
			if (b[i])
				bits.add(juce::String(i));

		return bits.joinIntoString(", ");
	}

	void dumpDeviceInfo()
	{
		logMessage("--------------------------------------");
		logMessage("Current audio device type: " + (deviceManager.getCurrentDeviceTypeObject() != nullptr
			? deviceManager.getCurrentDeviceTypeObject()->getTypeName()
			: "<none>"));

		if (auto* device = deviceManager.getCurrentAudioDevice())
		{
			logMessage("Current audio device: " + device->getName().quoted());
			logMessage("Sample rate: " + juce::String(device->getCurrentSampleRate()) + " Hz");
			logMessage("Block size: " + juce::String(device->getCurrentBufferSizeSamples()) + " samples");
			logMessage("Bit depth: " + juce::String(device->getCurrentBitDepth()));
			logMessage("Input channel names: " + device->getInputChannelNames().joinIntoString(", "));
			logMessage("Active input channels: " + getListOfActiveBits(device->getActiveInputChannels()));
			logMessage("Output channel names: " + device->getOutputChannelNames().joinIntoString(", "));
			logMessage("Active output channels: " + getListOfActiveBits(device->getActiveOutputChannels()));
		}
		else
		{
			logMessage("No audio device open");
		}
	}

	void logMessage(const juce::String& m)
	{
		diagnosticsBox.moveCaretToEnd();
		diagnosticsBox.insertTextAtCaret(m + juce::newLine);
	}

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
