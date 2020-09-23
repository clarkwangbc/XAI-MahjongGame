//
// Created by Clark on 2020/9/15.
//

#include "ConfidenceDlg.h"
#include "GameConfig.h"
#include <fstream>

using namespace std;

ConfidenceDlg::ConfidenceDlg() : IDialog()
{
	RegDialogCtrl("Button_Confirm", m_btnConfirm);
	RegDialogCtrl("TextField_Confidence", m_pConfidenceField);
	confidence = "";
}
ConfidenceDlg::~ConfidenceDlg() {
	
}

void ConfidenceDlg::onUILoaded() {
	m_rootNode->setScale(0.5f);
	m_rootNode->runAction(EaseElasticOut::create(ScaleTo::create(0.5f, 1.0f, 1.0f))); //¶¯»­
	m_rootNode->addChild(LayerColor::create(Color4B(0, 0, 0, 200)), -1);
	m_pConfidenceField->addEventListener([this](Ref*, ui::TextField::EventType type) {
		switch (type) {
		case ui::TextField::EventType::INSERT_TEXT:{
			if (m_pConfidenceField->getInsertText()) {
				m_pConfidenceField->setColor(Color3B(0,0,0));
				confidence = m_pConfidenceField->getString();
				m_pConfidenceField->setString(confidence);
			}
			break;
		}
		case ui::TextField::EventType::DELETE_BACKWARD: {
			confidence = m_pConfidenceField->getString();
			break;
		}
		default:
			break;
		}
	});
	m_btnConfirm->addClickEventListener([this](Ref* sender) {
		ofstream fconfidence;
		std::string filename = GameConfig::getInstance()->m_ParticipantId + "_confidence.txt";
		std::string confidence_path = "C:/Users/clark/MahjongGame/UserLogs/" + filename;
		fconfidence.open(confidence_path, ios::app);
		fconfidence << confidence << endl;
		fconfidence.close();
		DialogManager::shared()->closeCurrentDialog();
	});
}
