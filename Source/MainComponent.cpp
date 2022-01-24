#include "MainComponent.h"

float secondInput[44100];
double xposInput;
double yposInput;
int inputPos[2]{ 0, 0 };
int outputPos[2]{ 0, 0 };

//==============================================================================
MainComponent::MainComponent() : wavetableExciter_(10, &(wave[0]), wave.size()),
									audioSetupComp(deviceManager,
										0,     // minimum input channels
										256,   // maximum input channels
										0,     // minimum output channels
										256,   // maximum output channels
										false, // ability to select midi inputs
										false, // ability to select midi output device
										false, // treat channels as stereo pairs
										false) // hide advanced options
{
	addAndMakeVisible(audioSetupComp);
	//addAndMakeVisible(diagnosticsBox);

	diagnosticsBox.setMultiLine(true);
	diagnosticsBox.setReturnKeyStartsNewLine(true);
	diagnosticsBox.setReadOnly(true);
	diagnosticsBox.setScrollbarsShown(true);
	diagnosticsBox.setCaretVisible(false);
	diagnosticsBox.setPopupMenuEnabled(true);
	diagnosticsBox.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0x32ffffff));
	diagnosticsBox.setColour(juce::TextEditor::outlineColourId, juce::Colour(0x1c000000));
	diagnosticsBox.setColour(juce::TextEditor::shadowColourId, juce::Colour(0x16000000));

    // Make sure you set the size of the component after
    // you add any child components.
    setSize (800, 600);

    // Some platforms require permissions to open input channels so request that here
    if (juce::RuntimePermissions::isRequired (juce::RuntimePermissions::recordAudio)
        && ! juce::RuntimePermissions::isGranted (juce::RuntimePermissions::recordAudio))
    {
        juce::RuntimePermissions::request (juce::RuntimePermissions::recordAudio,
                                           [&] (bool granted) { setAudioChannels (granted ? 2 : 0, 2); });
    }
    else
    {
        // Specify the number of input and output channels that we want to open
        setAudioChannels (2, 2);
    }

	//Interface//
	addAndMakeVisible(lblDrumOne);
	addAndMakeVisible(sldPropagationOne);
	sldPropagationOne.setRange(0.0, 0.49, 0.0001);
	sldPropagationOne.setValue(0.085);
	sldPropagationOne.addListener(this);
	addAndMakeVisible(sldDampingOne);
	sldDampingOne.setRange(0.0, 0.001, 0.000001);
	sldDampingOne.setValue(0.001);
	sldDampingOne.addListener(this);

	addAndMakeVisible(lblDrumTwo);
	addAndMakeVisible(sldPropagationTwo);
	sldPropagationTwo.setRange(0.0, 0.49, 0.0001);
	sldPropagationTwo.setValue(0.085);
	sldPropagationTwo.addListener(this);
	addAndMakeVisible(sldDampingTwo);
	sldDampingTwo.setRange(0.0, 0.001, 0.000001);
	sldDampingTwo.setValue(0.001);
	sldDampingTwo.addListener(this);

	addAndMakeVisible(sldInputDuration);
	sldInputDuration.setRange(0.0, 1000, 1.0);
	sldInputDuration.setValue(10);
	sldInputDuration.addListener(this);

	//Labels//
	lblDrumOne.setText("Drum One", dontSendNotification);
	addAndMakeVisible(lblPropagationOne);
	lblPropagationOne.setText("Propagation", dontSendNotification);
	lblPropagationOne.attachToComponent(&sldPropagationOne, true);
	addAndMakeVisible(lblDampingOne);
	lblDampingOne.setText("Damping", dontSendNotification);
	lblDampingOne.attachToComponent(&sldDampingOne, true);
	lblDrumTwo.setText("Drum Two", dontSendNotification);
	addAndMakeVisible(lblPropagationTwo);
	lblPropagationTwo.setText("Propagation", dontSendNotification);
	lblPropagationTwo.attachToComponent(&sldPropagationTwo, true);
	addAndMakeVisible(lblDampingTwo);
	lblDampingTwo.setText("Damping", dontSendNotification);
	lblDampingTwo.attachToComponent(&sldDampingTwo, true);
	addAndMakeVisible(lblInputDuration);
	lblInputDuration.setText("Input Duration", dontSendNotification);
	lblInputDuration.attachToComponent(&sldInputDuration, true);

	addAndMakeVisible(lblFPS);
	lblFPS.setText("FPS: ", dontSendNotification);
	addAndMakeVisible(lblLatency);
	lblLatency.setText("Latency: ", dontSendNotification);

	mutexInit.lock();
	Implementation impl = OPENCL;
	unsigned int bufferFrames = 1024; // 256 sample frames
	const double gridSpacing = 0.001;
	simulationModel = new FDTD_Accelerated(impl, bufferFrames, gridSpacing);
	uint32_t inputPosition[2] = { 0, 0 };
	uint32_t outputPosition[2] = { 0, 0 };
	float boundaryValue = 1.0;
	simulationModel->createModel(physicalModelPath_, boundaryValue, inputPosition, outputPosition);
	// Update Coefficients.
	float propagationCoefficientOne = 0.0018;
	float dampingCoefficientOne = 0.00010;
	float propagationCoefficientTwo = 0.12;
	float dampingCoefficientTwo = 0.0008;

	simulationModel->updateCoefficient("muOne", 9, dampingCoefficientOne);
	simulationModel->updateCoefficient("lambdaOne", 10, propagationCoefficientOne);
	simulationModel->updateCoefficient("muTwo", 12, dampingCoefficientTwo);
	simulationModel->updateCoefficient("lambdaTwo", 11, propagationCoefficientOne);

	//Setup output position
	outputPos[0] = simulationModel->getModelHeight() / 2.0;
	outputPos[1] = simulationModel->getModelWidth() / 4.0;
	simulationModel->setOutputPosition(outputPos);
	outputPos[0] = simulationModel->getModelHeight() / 2.0;
	outputPos[1] = (simulationModel->getModelWidth() / 4.0) * 2.5;
	simulationModel->setOutputPosition(outputPos);

	//Timer callback
	startTimer(1);
	mutexInit.unlock();

	isInit = true;
}

MainComponent::~MainComponent()
{
    // This shuts down the audio device and clears the audio source.
    shutdownAudio();
}
//==============================================================================
void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    // This function will be called when the audio device is started, or when
    // its settings (i.e. sample rate, block size, etc) are changed.

    // You can use this function to initialise any resources you might need,
    // but be careful - it will be called on the audio thread, not the GUI thread.

    // For more details, see the help for AudioProcessor::prepareToPlay()
}

int counter = 0;
void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
	if (isInit)
	{
		mutexInit.lock();

		// Your audio-processing code goes here!
		auto level = 0.125f;
		auto* leftBuffer = bufferToFill.buffer->getWritePointer(0, bufferToFill.startSample);
		auto* rightBuffer = bufferToFill.buffer->getWritePointer(1, bufferToFill.startSample);

		//Input excitation//
		for (int j = 0; j != bufferToFill.numSamples; ++j)
		{
			//inputExcitation[j] = sineExciter_.getNextSample();
			if (wavetableExciter_.isExcitation())
				inputExcitation[j] = wavetableExciter_.getNextSample();
			else
				inputExcitation[j] = 0;
		}

		// For more details, see the help for AudioProcessor::getNextAudioBlock()
		simulationModel->fillBuffer(inputExcitation, leftBuffer, bufferToFill.numSamples);
		memcpy(rightBuffer, leftBuffer, bufferToFill.numSamples * sizeof(float));

		counter += (bufferToFill.numSamples);
		if (counter > framerate)
		{
			simulationModel->renderSimulation();
			counter = 0;
		}

		mutexInit.unlock();
	}
}

void MainComponent::releaseResources()
{
    // This will be called when the audio device stops, or when it is being
    // restarted due to a setting change.

    // For more details, see the help for AudioProcessor::releaseResources()
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    // You can add your drawing code here!
}

void MainComponent::resized()
{
	auto rect = getLocalBounds();

	audioSetupComp.setBounds(rect.removeFromLeft(proportionOfWidth(0.6f)));
	rect.reduce(10, 10);
	audioSetupComp.setVisible(false);

	auto topLine(rect.removeFromTop(20));
	cpuUsageLabel.setBounds(topLine.removeFromLeft(topLine.getWidth() / 2));
	cpuUsageText.setBounds(topLine);
	rect.removeFromTop(20);

	diagnosticsBox.setBounds(rect);
    // This is called when the MainContentComponent is resized.
    // If you add any child components, this is where you should
    // update their positions.
	uint32_t sliderLeft = 50;
	lblDrumOne.setBounds(getWidth()/2 - 30, 730, getWidth() - 60, 40);
	sldPropagationOne.setBounds(80, lblDrumOne.getY() + 40, lblDrumOne.getWidth() - sliderLeft - 10, 40);
	sldDampingOne.setBounds(sldPropagationOne.getX(), sldPropagationOne.getY() + 40, lblDrumOne.getWidth() - sliderLeft - 10, 40);
	lblDrumTwo.setBounds(getWidth() / 2 - 30, sldDampingOne.getY() + 40, getWidth() - 60, 40);
	sldPropagationTwo.setBounds(sldDampingOne.getX(), lblDrumTwo.getY() + 40, lblDrumOne.getWidth() - sliderLeft - 10, 40);
	sldDampingTwo.setBounds(sldPropagationTwo.getX(), sldPropagationTwo.getY() + 40, lblDrumOne.getWidth() - sliderLeft - 10, 40);
	sldInputDuration.setBounds(sldDampingTwo.getX(), sldDampingTwo.getY() + 40, lblDrumOne.getWidth() - sliderLeft - 10, 40);
}

//Interface//
void MainComponent::buttonClicked(Button* btn)
{
	
}
void MainComponent::sliderValueChanged(Slider* sld)
{

}
void MainComponent::sliderDragEnded(Slider* sld)
{
	if (sld == &sldPropagationOne)
	{
		//mutexSensel.lock();

		simulationModel->updateCoefficient("lambdaOne", 10, sldPropagationOne.getValue());

		//mutexSensel.unlock();
	}

	if (sld == &sldDampingOne)
	{
		//mutexSensel.lock();

		simulationModel->updateCoefficient("muOne", 9, sldDampingOne.getValue());

		//mutexSensel.unlock();
	}
	if (sld == &sldPropagationTwo)
	{
		//mutexSensel.lock();

		simulationModel->updateCoefficient("lambdaTwo", 11, sldPropagationTwo.getValue());

		//mutexSensel.unlock();
	}

	if (sld == &sldDampingTwo)
	{
		//mutexSensel.lock();

		simulationModel->updateCoefficient("muTwo", 12, sldDampingTwo.getValue());

		//mutexSensel.unlock();
	}

	if (sld == &sldInputDuration)
	{
		//mutexSensel.lock();

		exciteDuration = sldInputDuration.getValue();
		wavetableExciter_.setDuration(exciteDuration);

		//mutexSensel.unlock();
	}
}

void MainComponent::hiResTimerCallback()
{
	senselInterface.check();
	inputPos[0] = senselInterface.fingers[0].x * simulationModel->getModelWidth();
	inputPos[1] = senselInterface.fingers[0].y * simulationModel->getModelHeight();
	simulationModel->setInputPosition(inputPos);
	if (senselInterface.contactAmount > 0 && senselInterface.fingers[0].state == CONTACT_START)
	{
		wavetableExciter_.resetExcitation();
	}

	//auto cpu = deviceManager.getCpuUsage() * 100;
	//cpuUsageText.setText(juce::String(cpu, 6) + " %", juce::dontSendNotification);
}