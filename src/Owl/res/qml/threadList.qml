import QtQuick 2.7
import QtQuick.Controls 1.4
import QtQuick.Window 2.0
import QtGraphicalEffects 1.0
import reader.owl 1.0


Item
{
    id: rootItem

    property int smallTextSize: 11
    property int osTextSizeModifier: 5

    signal stickyDisplayChanged

    Component.onCompleted:
    {
        setHasThreads(false);
        ListView.currentIndex = -1;
    }

    Settings
    {
        id: threadListSettings
    }

    // gets called by ThreadViewWidget.cpp everytime new threads are loaded
    function setHasThreads(hasThreads)
    {
        threadScrollView.visible = hasThreads;
        noThreadsImage.visible = !hasThreads;
        fillRect.visible = !hasThreads;
        threadListView.currentIndex = -1;
    }

    Rectangle
    {
        id: fillRect
        anchors.fill: parent
    }

    Image
    {
        id: noThreadsImage
        opacity: 0.07
        anchors.horizontalCenter: rootItem.horizontalCenter
        anchors.verticalCenter: rootItem.verticalCenter
        source: "../images/owl-bg1.png"
    }

    Menu
    {
        id: threadViewContextMenu

        MenuItem
        {
            id: hideStickyMenuItem
            text: qsTr("Hide Stickies")

            onTriggered:
            {
                threadListPage.showStickies = !threadListPage.showStickies;
                threadListPage.refreshThreadDisplay();
                hideStickyMenuItem.text = threadListPage.showStickies
                        ? qsTr("Hide Stickies") : qsTr("Show Stickies");
                stickyDisplayChanged();
            }
        }

        MenuSeparator { }

        MenuItem
        {
            text: qsTr("Copy URL")
            onTriggered:
            {
                threadListPage.copyUrl(threadListView.currentIndex);
            }
        }

        MenuItem
        {
            text: qsTr("Open in Browser")
            onTriggered:
            {
                threadListPage.loadInBrowser(threadListView.currentIndex);
            }
        }
    }

    ScrollView
    {
        id: threadScrollView
        anchors.fill: parent;

        ListView
        {
            id: threadListView
            anchors.fill: parent
            model: threadListModel
            focus: true

            activeFocusOnTab: true
            boundsBehavior: Flickable.StopAtBounds
            highlightFollowsCurrentItem: true
            highlightMoveDuration: 100
            highlightMoveVelocity: 100
            flickableDirection: Flickable.VerticalFlick

            highlight: Rectangle { color: "#e4ebf1" }

            delegate: Item
            {
                id: threadListDelegate

                property int avatarRectWidth: 68
                property int spacerRectHeight: 10
                property int delegateSpacerHeight: 25

                width: parent.width

//                property int componentHeight:
//                    authorText.height + authorText.anchors.topMargin + authorText.anchors.bottomMargin +
//                    titleText.height + titleText.anchors.topMargin + titleText.anchors.bottomMargin +
//                    (previewText.visible ? previewText.height + 5 : 0) +
//                    spacerRect.height +
//                    (lastAuthorRect.visible ? lastAuthorRect.height : 0)

                //height: Math.max(158, componentHeight)
                height: delegateSpacerHeight
                        + titleText.height
                        + authorText.height
                        + previewTextItem.height
                        + lastReplyItem.height

                //visible: !model.modelData.sticky || threadListPage.showStickies

                Rectangle
                {
                    id: delegateFillRect
                    anchors.top: parent.top
                    width: parent.width
                    color: "red"
                    border.color: "black"
                    border.width: 5

                    Rectangle
                    {
                        id: avatarRect
                        anchors.top: parent.top
                        anchors.left: parent.left
                        height: threadListDelegate.height-spacerRect.height
                        width: threadListDelegate.avatarRectWidth
                        visible: threadListSettings.read("threadlist.avatars.visible") && model.modelData.iconUrl.length > 0

                        Image
                        {
                            id: lastAuthorImage
                            width: 52
                            height: 52
                            anchors.horizontalCenter: avatarRect.horizontalCenter
                            anchors.top: avatarRect.top
                            source: model.modelData.iconUrl
                            fillMode: Image.PreserveAspectCrop

                            onStatusChanged:
                            {
                                if (lastAuthorImage.status == Image.Error)
                                {
                                    lastAuthorImage.source = "qrc:/icons/no-avatar.png";
                                }
                            }

                            property bool rounded: true
                            property bool adapt: false

                            layer.enabled: rounded
                            layer.effect: OpacityMask
                            {
                                maskSource: Item
                                {
                                    width: lastAuthorImage.width
                                    height: lastAuthorImage.height
                                    Rectangle
                                    {
                                        anchors.centerIn: parent
                                        width: lastAuthorImage.adapt ? lastAuthorImage.width : Math.min(lastAuthorImage.width, lastAuthorImage.height)
                                        height: lastAuthorImage.adapt ? lastAuthorImage.height : width
                                        radius: Math.min(width, height)
                                    }
                                }
                            }
                        }

                        Image
                        {
                            id: stickyImage
                            anchors.topMargin: 2
                            anchors.top: parent.top
                            anchors.right: avatarRect.right
                            width: 24
                            height: 24
                            source: "/icons/pin.png"
                            visible: model.modelData.sticky
                        }
                    }

                    Rectangle
                    {
                        id: contentRect
                        anchors.top: parent.top
                        anchors.left: avatarRect.right
                        height: threadListDelegate.height-spacerRect.height
                        width: threadListDelegate.width-avatarRect.width

                        Text
                        {
                            id: titleText
                            anchors.top: parent.top
                            anchors.left: parent.left
                            width: parent.width
                            text: title
                            font.pointSize: 11 + rootItem.osTextSizeModifier;
                            font.bold: unread
                            wrapMode: Text.Wrap
                        }

                        Text
                        {
                            id: authorText
                            anchors.top: titleText.bottom
                            text: author
                            font.pointSize: 9 + rootItem.osTextSizeModifier;
                            color: "grey"
                            font.bold: true
                        }

                        Text
                        {
                            id: repliesText
                            anchors.top: titleText.bottom
                            anchors.left: authorText.right
                            text: " " + String.fromCharCode(8226) + " "
                                  + replyCount + " "
                                  + (replyCount == 1 ? qsTr("reply") : qsTr("replies"))
                            font.pointSize: 9 + rootItem.osTextSizeModifier;
                            color: "grey"
                        }

//                        Text
//                        {
//                            id: dateText
//                            anchors.top: titleText.bottom
//                            anchors.left: repliesText.right
//                            text: " " + String.fromCharCode(8226) + " " + model.modelData.dateText
//                            font.pointSize: 9;
//                            color: "grey"
//                        }

                        Text
                        {
                            id: previewTextItem
                            anchors.top: authorText.bottom
                            anchors.topMargin: 3
                            anchors.left: parent.left
                            width: parent.width
                            text: previewText
                            wrapMode: Text.Wrap
                            font.pointSize: 10 + rootItem.osTextSizeModifier;
                            color: "grey"
                        }

                        Text
                        {
                            id: lastReplyItem
                            anchors.top: previewTextItem.bottom
                            anchors.left: parent.left
                            anchors.topMargin: 3
                            width: parent.width
                            text: qsTr("<b>%1</b> replied %2")
                                .arg(lastAuthor)
                                .arg(model.modelData.dateText)
                            wrapMode: Text.Wrap
                            font.pointSize: 8 + rootItem.osTextSizeModifier;
//                            font.bold: true
                            color: "grey"
                        }
                    }

                    Rectangle
                    {
                        id: spacerRect
                        height: threadListDelegate.spacerRectHeight
                        width: delegateFillRect.width
                        anchors.top: avatarRect.bottom
                    }
                }

                MouseArea
                {
                    id: mouseArea
                    hoverEnabled: true
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton | Qt.RightButton

                    onClicked:
                    {
                        if (mouse.button === Qt.LeftButton)
                        {
                            if (threadListDelegate.ListView.view.currentIndex !== index)
                            {
                                threadListDelegate.ListView.view.currentIndex = index;
                                model.modelData.loadThread();
                            }
                        }
                    }

                    onPressed:
                    {
                        if (mouse.button === Qt.RightButton)
                        {
                            mouse.accepted = true;
                            threadViewContextMenu.popup();
                        }
                    }
                }
            } // Item
        }
    }
}
