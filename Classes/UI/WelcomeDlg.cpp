#include "WelcomeDlg.h"
#include "AlertDlg.h"
#include "PlayScene.h"
#include "GameConfig.h"

using namespace std;

WelcomeDlg::WelcomeDlg() : IDialog()
{
    RegDialogCtrl("Button_Logon", m_btnLogon);
	RegDialogCtrl("Participant_ID", m_tfdPartID);
}

WelcomeDlg::~WelcomeDlg(){
}

void WelcomeDlg::onUILoaded() {
	
	m_tfdPartID->addEventListener([this](Ref*, ui::TextField::EventType type) {
		switch (type) {
		case ui::TextField::EventType::INSERT_TEXT: {
			if (m_tfdPartID->getInsertText()) {
				m_tfdPartID->setPlaceHolder("");
				m_tfdPartID->setColor(Color3B(0, 0, 0));
				GameConfig::getInstance()->m_ParticipantId = m_tfdPartID->getString();
			}
			break;
		}
		case ui::TextField::EventType::DELETE_BACKWARD: {
			if (m_tfdPartID->getInsertText()) {
				GameConfig::getInstance()->m_ParticipantId = m_tfdPartID->getString();
			}
			break;
		}
		default:
			break;
		}
	});
    m_btnLogon->addClickEventListener([this](Ref* sender) {
		GameConfig::getInstance()->saveConfig();    //关闭前保存配置
        auto alert = AlertDlg::create();
        alert->setAlertType(AlertDlg::ENUM_ALERT);
        alert->setCallback([]() {
			
            DialogManager::shared()->closeAllDialog();
            Director::getInstance()->replaceScene(PlayScene::create());
        }, nullptr);
        alert->setText( "欢迎参加IHFE麻将游戏实验！\n请在充分了解实验流程和游戏规则后开始游戏" );
        DialogManager::shared()->showDialog(alert);
    });
}

