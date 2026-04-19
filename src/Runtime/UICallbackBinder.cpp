#include "Runtime/UICallbackBinder.hpp"

#include "UI/UIManager.hpp"

void UICallbackBinder::bind(UIManager& uiManager, const UICallbackBindings& bindings) {
    uiManager.gameMenu().setOnResume(bindings.gameMenu.onResume);
    uiManager.gameMenu().setOnSave(bindings.gameMenu.onSave);
    uiManager.gameMenu().setOnQuitToMainMenu(bindings.gameMenu.onQuitToMainMenu);

    uiManager.mainMenu().setOnLoadSaves(bindings.mainMenu.onLoadSaves);
    uiManager.mainMenu().setOnExitGame(bindings.mainMenu.onExitGame);
    uiManager.mainMenu().setOnCreateSave(bindings.mainMenu.onCreateSave);
    uiManager.mainMenu().setOnPlaySave(bindings.mainMenu.onPlaySave);
    uiManager.mainMenu().setOnJoinMultiplayer(bindings.mainMenu.onJoinMultiplayer);
    uiManager.mainMenu().setOnDeleteSave(bindings.mainMenu.onDeleteSave);
    uiManager.mainMenu().setOnEditSave(bindings.mainMenu.onEditSave);

    uiManager.hud().setOnMenu(bindings.hud.onMenu);
    uiManager.hud().setOnResetTurn(bindings.hud.onResetTurn);
    uiManager.hud().setOnEndTurn(bindings.hud.onEndTurn);

    uiManager.toolBar().setOnSelect(bindings.toolBar.onSelect);
    uiManager.toolBar().setOnBuild(bindings.toolBar.onBuild);
    uiManager.toolBar().setOnOverview(bindings.toolBar.onOverview);

    uiManager.buildToolPanel().setOnSelectBuildType(bindings.panels.onSelectBuildType);
    uiManager.buildingPanel().setOnCancelConstruction(bindings.panels.onCancelBuildingConstruction);
    uiManager.barracksPanel().setOnCancelConstruction(bindings.panels.onCancelBarracksConstruction);
    uiManager.piecePanel().setOnUpgrade(bindings.panels.onPieceUpgrade);
    uiManager.piecePanel().setOnDisband(bindings.panels.onPieceDisband);
    uiManager.barracksPanel().setOnProduce(bindings.panels.onBarracksProduce);
}