import QtQuick

Item {
    id: root
    anchors.fill: parent
    z: 25
    clip: true

    property bool finished: false
    visible: !finished

    Item {
        id: catContainer
        width: 140
        height: 90
        x: (root.width - width) / 2
        y: (root.height - height) / 2
        opacity: 0.0

        property real eyeScaleY: 1.0
        property real tailAngle: 0.0
        property real pawOffset: 0.0
        property real earAngle: 0.0

        Canvas {
            id: catCanvas
            anchors.fill: parent
            antialiasing: true

            onPaint: {
                var ctx = getContext("2d");
                ctx.clearRect(0, 0, width, height);

                var strokeCol = theme.accent || "#fabd2f";
                ctx.strokeStyle = strokeCol;
                ctx.fillStyle = strokeCol;
                ctx.lineWidth = 2.2;
                ctx.lineCap = "round";
                ctx.lineJoin = "round";

                // 1. Sleek Minimal Cat Body (horizontal oval/curve)
                ctx.beginPath();
                ctx.ellipse(45, 48, 32, 20, 0, 0, 2 * Math.PI);
                ctx.stroke();

                // 2. Head (circle)
                ctx.beginPath();
                ctx.arc(88, 40, 18, 0, 2 * Math.PI);
                ctx.stroke();

                // 3. Left Ear
                ctx.beginPath();
                ctx.moveTo(76, 28);
                ctx.lineTo(79, 13);
                ctx.lineTo(89, 23);
                ctx.stroke();

                // 4. Right Ear (with tiny alert earAngle)
                ctx.beginPath();
                ctx.moveTo(93, 23);
                ctx.lineTo(104 + catContainer.earAngle, 14);
                ctx.lineTo(105, 30);
                ctx.stroke();

                // 5. Blinking Eyes
                ctx.save();
                ctx.translate(86, 38);
                ctx.scale(1.0, catContainer.eyeScaleY);
                ctx.beginPath();
                ctx.arc(0, 0, 2.2, 0, 2 * Math.PI);
                ctx.fill();
                ctx.restore();

                ctx.save();
                ctx.translate(97, 38);
                ctx.scale(1.0, catContainer.eyeScaleY);
                ctx.beginPath();
                ctx.arc(0, 0, 2.2, 0, 2 * Math.PI);
                ctx.fill();
                ctx.restore();

                // 6. Tiny Nose & Cute Mouth
                ctx.beginPath();
                ctx.moveTo(91, 42);
                ctx.lineTo(93, 44);
                ctx.lineTo(95, 42);
                ctx.moveTo(93, 44);
                ctx.lineTo(93, 47);
                ctx.stroke();

                // 7. Whiskers (left & right)
                ctx.beginPath();
                // Left whiskers
                ctx.moveTo(78, 42); ctx.lineTo(65, 41);
                ctx.moveTo(78, 45); ctx.lineTo(64, 47);
                // Right whiskers
                ctx.moveTo(101, 42); ctx.lineTo(114, 41);
                ctx.moveTo(101, 45); ctx.lineTo(115, 47);
                ctx.stroke();

                // 8. Animated Scampering Paws
                ctx.beginPath();
                // Front paws
                ctx.moveTo(68 + catContainer.pawOffset, 66);
                ctx.lineTo(68 + catContainer.pawOffset, 78);
                ctx.moveTo(78 - catContainer.pawOffset, 66);
                ctx.lineTo(78 - catContainer.pawOffset, 78);
                // Back paws
                ctx.moveTo(26 - catContainer.pawOffset, 64);
                ctx.lineTo(26 - catContainer.pawOffset, 76);
                ctx.moveTo(38 + catContainer.pawOffset, 65);
                ctx.lineTo(38 + catContainer.pawOffset, 77);
                ctx.stroke();

                // 9. Animated Curved Tail
                ctx.beginPath();
                ctx.moveTo(18, 52);
                var tX = Math.sin(catContainer.tailAngle) * 16;
                var tY = Math.cos(catContainer.tailAngle) * 8;
                ctx.bezierCurveTo(8 + tX, 40 + tY, 4 - tX, 24, 12 + tX, 16);
                ctx.stroke();
            }
        }

        Connections {
            target: catContainer
            function onTailAngleChanged() { catCanvas.requestPaint(); }
            function onPawOffsetChanged() { catCanvas.requestPaint(); }
            function onEyeScaleYChanged() { catCanvas.requestPaint(); }
            function onEarAngleChanged() { catCanvas.requestPaint(); }
        }
    }

    // Startup Easter Egg Animation Sequence
    SequentialAnimation {
        id: startupCatSequence
        running: true

        // 1. Cat appears smoothly at center
        ParallelAnimation {
            NumberAnimation {
                target: catContainer
                property: "opacity"
                from: 0.0
                to: 1.0
                duration: 400
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: catContainer
                property: "scale"
                from: 0.75
                to: 1.0
                duration: 400
                easing.type: Easing.OutBack
            }
        }

        // 2. Swish tail softly while sitting
        NumberAnimation {
            target: catContainer
            property: "tailAngle"
            from: -0.4
            to: 0.4
            duration: 350
            easing.type: Easing.InOutSine
        }

        // 3. Cute blink
        SequentialAnimation {
            NumberAnimation {
                target: catContainer
                property: "eyeScaleY"
                from: 1.0
                to: 0.1
                duration: 75
            }
            NumberAnimation {
                target: catContainer
                property: "eyeScaleY"
                from: 0.1
                to: 1.0
                duration: 90
            }
        }

        // 4. Alert! Notice someone is watching (ears perk up, tiny hop)
        ParallelAnimation {
            NumberAnimation {
                target: catContainer
                property: "earAngle"
                from: 0.0
                to: 4.0
                duration: 120
            }
            SequentialAnimation {
                NumberAnimation {
                    target: catContainer
                    property: "y"
                    to: catContainer.y - 12
                    duration: 100
                    easing.type: Easing.OutQuad
                }
                NumberAnimation {
                    target: catContainer
                    property: "y"
                    to: catContainer.y
                    duration: 100
                    easing.type: Easing.InQuad
                }
            }
        }

        // 5. Run away across the canvas and hide!
        ParallelAnimation {
            // Scampering paws loop
            SequentialAnimation {
                loops: 5
                NumberAnimation {
                    target: catContainer
                    property: "pawOffset"
                    from: -5.0
                    to: 5.0
                    duration: 70
                }
                NumberAnimation {
                    target: catContainer
                    property: "pawOffset"
                    from: 5.0
                    to: -5.0
                    duration: 70
                }
            }

            // Tail trailing back while running
            NumberAnimation {
                target: catContainer
                property: "tailAngle"
                to: -1.0
                duration: 200
            }

            // Sprint to right edge of screen and hide
            NumberAnimation {
                target: catContainer
                property: "x"
                to: root.width + 120
                duration: 700
                easing.type: Easing.InQuad
            }

            // Fade out as it exits
            SequentialAnimation {
                PauseAnimation { duration: 400 }
                NumberAnimation {
                    target: catContainer
                    property: "opacity"
                    to: 0.0
                    duration: 300
                }
            }
        }

        // 6. Finished: hide completely
        ScriptAction {
            script: {
                root.finished = true;
                root.visible = false;
            }
        }
    }
}

