//
// Created by Clark on 2020/9/15.
//

#include "RecordDlg.h"
#include "GameConfig.h"

using namespace std;

RecordDlg::RecordDlg() : IDialog()
{
	RegDialogCtrl("Button_Close", m_btnClose);
	RegDialogCtrl("Slider_Volume", m_pSliderVolume);
}
RecordDlg::~RecordDlg() {
}

void RecordDlg::onUILoaded() {
	m_rootNode->setScale(0.5f);
	m_rootNode->runAction(EaseElasticOut::create(ScaleTo::create(0.5f, 1.0f, 1.0f))); //����
	m_rootNode->addChild(LayerColor::create(Color4B(0, 0, 0, 200)), -1);
	m_btnClose->addClickEventListener([this](Ref* sender) {
		GameConfig::getInstance()->saveConfig();    //�ر�ǰ��������
		DialogManager::shared()->closeCurrentDialog();
	});
	m_pSliderVolume->setMaxPercent(100);
	m_pSliderVolume->setPercent(static_cast<int>(GameConfig::getInstance()->m_EffectsVolume * 100));
	//�����ƶ��¼�
	m_pSliderVolume->addEventListener([this](Ref*, ui::Slider::EventType type) {
		switch (type) {
		case ui::Slider::EventType::ON_PERCENTAGE_CHANGED: {    //�������仯
			int currPercent = m_pSliderVolume->getPercent();    //��ǰ�ٷֱ�
			int maxPercent = m_pSliderVolume->getMaxPercent();  //���ٷֱ�
			GameConfig::getInstance()->m_EffectsVolume = (float)currPercent / (float)maxPercent;
			break;
		}
		default:
			break;
		}
	});
}
