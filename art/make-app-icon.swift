// Draws the app icon: RA (blue) and Dec (orange) guide traces settling into
// the OpenAstro logo, on a night-blue rounded square.
//
// usage: swift art/make-app-icon.swift art/openastro-logo.png OUTPUT_DIR
//
// Writes, in OUTPUT_DIR:
// - openastro-guide-insight-macos.png: 1024 px on Apple's icon grid, the
//   source for the macOS .icns
// - openastro-guide-insight.png: 512 px edge to edge, for Linux (the
//   AppImage tools take icons up to 512 px)
// - openastro-guide-insight.ico: 16 to 256 px, edge to edge, for Windows

import CoreGraphics
import Foundation
import ImageIO
import UniformTypeIdentifiers

let args = CommandLine.arguments
guard args.count == 3 else {
    FileHandle.standardError.write("usage: swift make-app-icon.swift LOGO.png OUTPUT_DIR\n".data(using: .utf8)!)
    exit(2)
}
let logoURL = URL(fileURLWithPath: args[1])
let outDir = URL(fileURLWithPath: args[2])

guard let logoSource = CGImageSourceCreateWithURL(logoURL as CFURL, nil),
      let logo = CGImageSourceCreateImageAtIndex(logoSource, 0, nil) else {
    FileHandle.standardError.write("cannot read \(logoURL.path)\n".data(using: .utf8)!)
    exit(1)
}

func rgb(_ hex: UInt32, _ alpha: CGFloat = 1) -> CGColor {
    CGColor(srgbRed: CGFloat((hex >> 16) & 0xff) / 255, green: CGFloat((hex >> 8) & 0xff) / 255,
            blue: CGFloat(hex & 0xff) / 255, alpha: alpha)
}

// The design is drawn on a 512-unit square with y down, like the SVG it
// came from. `artwork` is the square's size in pixels and `inset` its
// distance from the image's edge.
func draw(size: Int, artwork: CGFloat, inset: CGFloat, shadow: Bool) -> CGImage {
    let ctx = CGContext(data: nil, width: size, height: size, bitsPerComponent: 8, bytesPerRow: 0,
                        space: CGColorSpace(name: CGColorSpace.sRGB)!,
                        bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)!
    ctx.interpolationQuality = .high
    ctx.translateBy(x: 0, y: CGFloat(size))
    ctx.scaleBy(x: 1, y: -1)
    ctx.translateBy(x: inset, y: inset)
    let k = artwork / 512
    ctx.scaleBy(x: k, y: k)

    let square = CGPath(roundedRect: CGRect(x: 0, y: 0, width: 512, height: 512),
                        cornerWidth: 115, cornerHeight: 115, transform: nil)

    // the background, with a soft shadow on Apple's grid
    ctx.saveGState()
    if shadow {
        ctx.setShadow(offset: CGSize(width: 0, height: 6), blur: 14, color: rgb(0x000000, 0.35))
    }
    ctx.addPath(square)
    ctx.setFillColor(rgb(0x0b1220))
    ctx.fillPath()
    ctx.restoreGState()

    // a blue glow behind the logo
    ctx.saveGState()
    ctx.addPath(square)
    ctx.clip()
    let glow = CGGradient(colorsSpace: CGColorSpace(name: CGColorSpace.sRGB)!,
                          colors: [rgb(0x2f5bb8, 0.75), rgb(0x2f5bb8, 0)] as CFArray, locations: [0, 1])!
    ctx.drawRadialGradient(glow, startCenter: CGPoint(x: 379, y: 256), startRadius: 0,
                           endCenter: CGPoint(x: 379, y: 256), endRadius: 307, options: [])
    ctx.restoreGState()

    // the zero line
    ctx.saveGState()
    ctx.setStrokeColor(rgb(0xffffff, 0.16))
    ctx.setLineWidth(4)
    ctx.setLineDash(phase: 0, lengths: [10, 12])
    ctx.move(to: CGPoint(x: 40, y: 256))
    ctx.addLine(to: CGPoint(x: 300, y: 256))
    ctx.strokePath()
    ctx.restoreGState()

    // a trace: zero crossings at these x, peaks of these heights (up is negative)
    func trace(_ peaks: [CGFloat], color: CGColor, width: CGFloat) {
        let xs: [CGFloat] = [44, 94, 144, 190, 226, 254]
        let path = CGMutablePath()
        path.move(to: CGPoint(x: xs[0], y: 256))
        for i in 0..<peaks.count {
            let mid = (xs[i] + xs[i + 1]) / 2
            path.addQuadCurve(to: CGPoint(x: xs[i + 1], y: 256), control: CGPoint(x: mid, y: 256 + 2 * peaks[i]))
        }
        path.addLine(to: CGPoint(x: 280, y: 256))
        ctx.addPath(path)
        ctx.setStrokeColor(color)
        ctx.setLineWidth(width)
        ctx.setLineCap(.round)
        ctx.setLineJoin(.round)
        ctx.strokePath()
    }
    trace([70, -55, 35, -18, 6], color: rgb(0xff9a4d), width: 11)    // Dec
    trace([-120, 100, -70, 40, -16], color: rgb(0x5b9bff), width: 15) // RA

    // the OpenAstro logo, the right way up
    ctx.saveGState()
    ctx.translateBy(x: 256, y: 132 + 248)
    ctx.scaleBy(x: 1, y: -1)
    ctx.draw(logo, in: CGRect(x: 0, y: 0, width: 248, height: 248))
    ctx.restoreGState()

    return ctx.makeImage()!
}

func png(_ image: CGImage) -> Data {
    let data = NSMutableData()
    let dest = CGImageDestinationCreateWithData(data as CFMutableData, UTType.png.identifier as CFString, 1, nil)!
    CGImageDestinationAddImage(dest, image, nil)
    CGImageDestinationFinalize(dest)
    return data as Data
}

// macOS: 824 px artwork centred in 1024, as Apple's icon grid has it
let mac = draw(size: 1024, artwork: 824, inset: 100, shadow: true)
try png(mac).write(to: outDir.appendingPathComponent("openastro-guide-insight-macos.png"))

// Linux: edge to edge, like the Windows icon
try png(draw(size: 512, artwork: 512, inset: 0, shadow: false))
    .write(to: outDir.appendingPathComponent("openastro-guide-insight.png"))

// Windows: an .ico of PNG images, each drawn at its own size
let sizes = [16, 24, 32, 48, 64, 128, 256]
let images = sizes.map { png(draw(size: $0, artwork: CGFloat($0), inset: 0, shadow: false)) }
var ico = Data()
func le16(_ v: Int) { ico.append(UInt8(v & 0xff)); ico.append(UInt8((v >> 8) & 0xff)) }
func le32(_ v: Int) { le16(v & 0xffff); le16((v >> 16) & 0xffff) }
le16(0); le16(1); le16(sizes.count)
var offset = 6 + 16 * sizes.count
for (size, image) in zip(sizes, images) {
    ico.append(UInt8(size >= 256 ? 0 : size))
    ico.append(UInt8(size >= 256 ? 0 : size))
    ico.append(0); ico.append(0)
    le16(1); le16(32)
    le32(image.count); le32(offset)
    offset += image.count
}
for image in images { ico.append(image) }
try ico.write(to: outDir.appendingPathComponent("openastro-guide-insight.ico"))
print("wrote the macOS, Linux and Windows icons")
