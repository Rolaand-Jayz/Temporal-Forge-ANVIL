// Scalable vector icon set drawn by QtQuick.Canvas; no emoji, fonts, or SVG plugins.
// The 24x24 geometry is shared by the transport controls and settings UI.
import QtQuick

Item {
    id: root
    property string name: ""
    property color color: "#f1f6f5"
    implicitWidth: 22
    implicitHeight: 22
    onNameChanged: canvas.requestPaint()
    onColorChanged: canvas.requestPaint()
    onWidthChanged: canvas.requestPaint()
    onHeightChanged: canvas.requestPaint()

    Canvas {
        id: canvas
        anchors.fill: parent
        renderTarget: Canvas.Image
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.clearRect(0, 0, width, height) // Remove the prior glyph on state change.
            ctx.scale(width / 24, height / 24)
            ctx.strokeStyle = root.color
            ctx.fillStyle = root.color
            ctx.lineWidth = 1.9
            ctx.lineCap = "round"
            ctx.lineJoin = "round"

            function path(points, close, fill) {
                ctx.beginPath()
                ctx.moveTo(points[0], points[1])
                for (let i = 2; i < points.length; i += 2)
                    ctx.lineTo(points[i], points[i + 1])
                if (close) ctx.closePath()
                if (fill) ctx.fill()
                else ctx.stroke()
            }
            function segment(x1, y1, x2, y2) {
                path([x1, y1, x2, y2], false, false)
            }
            function box(x, y, w, h) {
                ctx.strokeRect(x, y, w, h)
            }
            function circle(x, y, r) {
                ctx.beginPath()
                ctx.arc(x, y, r, 0, Math.PI * 2)
                ctx.stroke()
            }
            const n = root.name
            if (n === "play") {
                path([8, 5, 19, 12, 8, 19], true, true)
            } else if (n === "pause") {
                ctx.fillRect(7, 5, 3.5, 14)
                ctx.fillRect(13.5, 5, 3.5, 14)
            } else if (n === "skip-back") {
                segment(5, 5, 5, 19)
                path([18, 5, 7, 12, 18, 19], true, true)
            } else if (n === "skip-forward") {
                segment(19, 5, 19, 19)
                path([6, 5, 17, 12, 6, 19], true, true)
            } else if (n === "rewind") {
                path([12, 5, 3, 12, 12, 19], true, true)
                path([21, 5, 12, 12, 21, 19], true, true)
            } else if (n === "fast-forward") {
                path([3, 5, 12, 12, 3, 19], true, true)
                path([12, 5, 21, 12, 12, 19], true, true)
            } else if (n === "step-back") {
                segment(6, 5, 6, 19)
                path([18, 6, 9, 12, 18, 18], true, false)
            } else if (n === "step-forward") {
                segment(18, 5, 18, 19)
                path([6, 6, 15, 12, 6, 18], true, false)
            } else if (n === "folder-open") {
                path([3, 8, 3, 5, 10, 5, 12, 8, 21, 8, 21, 10], false, false)
                path([3, 10, 20, 10, 17, 19, 3, 19, 3, 10], true, false)
            } else if (n === "volume-x" || n === "volume-low" || n === "volume-high") {
                path([3, 9, 7, 9, 12, 5, 12, 19, 7, 15, 3, 15], true, false)
                if (n === "volume-x") {
                    segment(16, 9, 21, 15)
                    segment(21, 9, 16, 15)
                } else {
                    ctx.beginPath(); ctx.arc(12, 12, 5.5, -0.85, 0.85); ctx.stroke()
                    if (n === "volume-high") {
                        ctx.beginPath(); ctx.arc(12, 12, 9, -0.73, 0.73); ctx.stroke()
                    }
                }
            } else if (n === "playlist") {
                segment(9, 6, 21, 6)
                segment(9, 12, 21, 12)
                segment(9, 18, 21, 18)
                path([3, 5, 6, 6, 3, 7], true, true)
                path([3, 11, 6, 12, 3, 13], true, true)
                path([3, 17, 6, 18, 3, 19], true, true)
            } else if (n === "compare") {
                box(3, 4, 18, 16)
                segment(12, 4, 12, 20)
                segment(6, 10, 9, 7)
                segment(6, 10, 9, 13)
                segment(18, 14, 15, 11)
                segment(18, 14, 15, 17)
            } else if (n === "maximize") {
                path([9, 4, 4, 4, 4, 9], false, false)
                path([15, 4, 20, 4, 20, 9], false, false)
                path([4, 15, 4, 20, 9, 20], false, false)
                path([20, 15, 20, 20, 15, 20], false, false)
            } else if (n === "minimize") {
                path([4, 9, 9, 9, 9, 4], false, false)
                path([15, 4, 15, 9, 20, 9], false, false)
                path([4, 15, 9, 15, 9, 20], false, false)
                path([15, 20, 15, 15, 20, 15], false, false)
            } else if (n === "settings") {
                circle(12, 12, 3.3)
                for (let a = 0; a < 8; ++a) {
                    const rad = a * Math.PI / 4
                    segment(12 + 6.5 * Math.cos(rad), 12 + 6.5 * Math.sin(rad),
                            12 + 9 * Math.cos(rad), 12 + 9 * Math.sin(rad))
                }
            } else if (n === "close") {
                segment(5, 5, 19, 19); segment(19, 5, 5, 19)
            } else if (n === "chevron-down") {
                path([5, 9, 12, 16, 19, 9], false, false)
            } else if (n === "sliders") {
                segment(4, 6, 20, 6); segment(4, 12, 20, 12); segment(4, 18, 20, 18)
                circle(9, 6, 2); circle(16, 12, 2); circle(10, 18, 2)
            } else if (n === "color") {
                circle(12, 12, 9)
                segment(12, 3, 12, 21)
                segment(4.2, 8, 19.8, 16)
            } else if (n === "cpu") {
                box(6, 6, 12, 12); box(9, 9, 6, 6)
                for (let i = 0; i < 4; ++i) {
                    const k = 8 + i * 2.7
                    segment(k, 3, k, 6); segment(k, 18, k, 21)
                    segment(3, k, 6, k); segment(18, k, 21, k)
                }
            } else if (n === "headphones") {
                ctx.beginPath(); ctx.arc(12, 12, 8, Math.PI, 0); ctx.stroke()
                box(4, 12, 3, 7); box(17, 12, 3, 7)
            } else if (n === "monitor") {
                box(3, 4, 18, 13)
                segment(12, 17, 12, 21); segment(8, 21, 16, 21)
            } else if (n === "keyboard") {
                box(2, 5, 20, 14)
                for (let i = 0; i < 5; ++i) {
                    segment(5 + i * 3.4, 9, 5 + i * 3.4, 10)
                }
                segment(7, 15, 17, 15)
            } else if (n === "warning") {
                path([12, 3, 22, 20, 2, 20], true, false)
                segment(12, 9, 12, 14)
                circle(12, 17, 0.65)
            } else if (n === "info") {
                circle(12, 12, 9); segment(12, 11, 12, 17); circle(12, 7.5, 0.65)
            } else if (n === "refresh") {
                ctx.beginPath(); ctx.arc(12, 12, 8, -2.6, 2.2); ctx.stroke()
                path([17, 4, 21, 5, 20, 10], false, false)
            }
        }
    }
}
