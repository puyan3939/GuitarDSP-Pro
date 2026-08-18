#include "AmpPage.h"

AmpPage::AmpPage(AmpEngine& engine) : ampEngine(engine)
{
    title.setText("AMP / 20-STAGE LAB", juce::dontSendNotification);
    title.setFont(juce::Font(26.0f, juce::Font::bold));
    title.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(title);

    for (int i = 0; i < AmpEngine::numStages; ++i)
        stageSelector.addItem(juce::String(i + 1).paddedLeft('0', 2) + "  " + AmpEngine::getStageName(i), i + 1);
    stageSelector.setSelectedId(1, juce::dontSendNotification);
    stageSelector.onChange = [this] { loadSelectedStage(); };
    addAndMakeVisible(stageSelector);

    role.setColour(juce::Label::textColourId, juce::Colour::fromRGB(205, 211, 219));
    role.setJustificationType(juce::Justification::topLeft);
    role.setMinimumHorizontalScale(0.8f);
    addAndMakeVisible(role);

    listenFor.setColour(juce::Label::textColourId, juce::Colour::fromRGB(222, 110, 58));
    addAndMakeVisible(listenFor);

    auto setup = [this](juce::Slider& s, const juce::String& name, double min, double max, double step)
    {
        s.setName(name);
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 86, 22);
        s.setRange(min, max, step);
        s.onValueChange = [this] { pushSelectedStage(); };
        addAndMakeVisible(s);
    };

    setup(preHp, "PRE HPF", 10.0, 1200.0, 1.0); preHp.setTextValueSuffix(" Hz");
    setup(preLp, "PRE LPF", 1500.0, 22000.0, 10.0); preLp.setTextValueSuffix(" Hz");
    setup(drive, "DRIVE", 0.25, 12.0, 0.01);
    setup(bias, "BIAS", -0.35, 0.35, 0.001);
    setup(postLp, "POST LPF", 1200.0, 22000.0, 10.0); postLp.setTextValueSuffix(" Hz");
    setup(output, "OUTPUT", 0.05, 2.0, 0.001);
    setup(nonlinear, "NONLINEAR", 0.0, 1.0, 0.001);
    setup(clipShape, "CLIP SHAPE", 0.0, 1.0, 0.001);

    loadSelectedStage();
}

void AmpPage::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(12, 16, 21));
    g.setColour(juce::Colour::fromRGB(28, 34, 42));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(10.0f), 12.0f, 1.0f);

    g.setColour(juce::Colour::fromRGB(150, 158, 168));
    g.setFont(12.0f);
    const juce::Slider* ss[] = { &preHp,&preLp,&drive,&bias,&postLp,&output,&nonlinear,&clipShape };
    for (auto* s : ss)
        g.drawText(s->getName(), s->getBounds().withHeight(18).translated(0, -17), juce::Justification::centred);
}

void AmpPage::resized()
{
    auto r = getLocalBounds().reduced(24);
    title.setBounds(r.removeFromTop(38));
    stageSelector.setBounds(r.removeFromTop(38).removeFromLeft(440));
    r.removeFromTop(8);
    role.setBounds(r.removeFromTop(48));
    listenFor.setBounds(r.removeFromTop(28));
    r.removeFromTop(22);

    const int w = r.getWidth() / 8;
    juce::Slider* ss[] = { &preHp,&preLp,&drive,&bias,&postLp,&output,&nonlinear,&clipShape };
    for (auto* s : ss)
        s->setBounds(r.removeFromLeft(w).reduced(6, 14));
}

void AmpPage::loadSelectedStage()
{
    const int i = juce::jlimit(0, AmpEngine::numStages - 1, stageSelector.getSelectedId() - 1);
    const auto& p = ampEngine.getParameters()[(size_t)i];
    updating = true;
    preHp.setValue(p.preHpHz, juce::dontSendNotification); preLp.setValue(p.preLpHz, juce::dontSendNotification);
    drive.setValue(p.drive, juce::dontSendNotification); bias.setValue(p.bias, juce::dontSendNotification);
    postLp.setValue(p.postLpHz, juce::dontSendNotification); output.setValue(p.output, juce::dontSendNotification);
    nonlinear.setValue(p.nonlinear, juce::dontSendNotification); clipShape.setValue(p.clipShape, juce::dontSendNotification);
    updating = false;
    role.setText("ROLE  |  " + juce::String(AmpEngine::getStageRole(i)), juce::dontSendNotification);
    listenFor.setText("LISTEN FOR  |  " + juce::String(AmpEngine::getStageListenFor(i)), juce::dontSendNotification);
}

void AmpPage::pushSelectedStage()
{
    if (updating) return;
    const int i = juce::jlimit(0, AmpEngine::numStages - 1, stageSelector.getSelectedId() - 1);
    auto p = ampEngine.getParameters()[(size_t)i];
    p.preHpHz=(float)preHp.getValue(); p.preLpHz=(float)preLp.getValue(); p.drive=(float)drive.getValue();
    p.bias=(float)bias.getValue(); p.postLpHz=(float)postLp.getValue(); p.output=(float)output.getValue();
    p.nonlinear=(float)nonlinear.getValue(); p.clipShape=(float)clipShape.getValue();
    ampEngine.setStageParameters(i,p);
}
