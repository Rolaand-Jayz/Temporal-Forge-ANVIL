// Contract-level test of atomic same-frame capture and fail-closed pairing.
#include "core/ReviewPairCapture.hpp"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QTemporaryDir>
#include <cassert>
#include <iostream>
#include <vector>

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir tmp;
    assert(tmp.isValid());
    anvil::ReviewPairCapture recorder(tmp.path().toStdString(), {48});
    assert(recorder.enabled() && recorder.wants(48) && !recorder.wants(49));
    anvil::ReviewFrameMeta m;
    m.frameIndex=48; m.ptsUs=1'600'000; m.ptsTicks=48;
    m.tbNum=1; m.tbDen=30; m.sourceW=16; m.sourceH=16;
    m.anvilStageMs=2.3; m.fsrDispatchMs=4.1;
    m.sourceId="fixture.mov"; m.gitSha=std::string(40,'a');
    std::vector<uint8_t> raw(16*16*4, 255), fsr(32*32*4, 255);
    for(size_t i=0;i<raw.size();i+=4) {
        raw[i]=static_cast<uint8_t>((i/4)%127);
        raw[i+1]=73; raw[i+2]=149;
    }
    QString error;
    assert(!recorder.save(m,raw,16,16,fsr,32,31,&error));
    assert(recorder.wants(48));
    assert(!recorder.save(m,raw,16,18,fsr,32,32,&error));
    assert(recorder.save(m,raw,16,16,fsr,32,32,&error));
    assert(!recorder.wants(48));
    assert(!recorder.save(m,raw,16,16,fsr,32,32,&error));
    const QString path = QDir(tmp.path()).filePath("frame_000048");
    const QImage imageA(QDir(path).filePath("anvil_baseline.png"));
    const QImage imageB(QDir(path).filePath("anvil_plus_fsr41.png"));
    assert(imageA.width()==32 && imageA.height()==32);
    assert(imageB.width()==32 && imageB.height()==32);
    QFile file(QDir(path).filePath("manifest.json"));
    assert(file.open(QIODevice::ReadOnly));
    const QJsonObject json = QJsonDocument::fromJson(file.readAll()).object();
    assert(json.value("schema").toString()=="anvil-visual-pair-v1");
    assert(json.value("verified_same_frame").toBool());
    assert(json.value("pts_ticks").toInteger()==48);
    assert(json.value("a").toObject().value("status").toString()=="baseline");
    assert(json.value("b").toObject().value("status").toString()=="tryout");
    assert(json.value("a").toObject().value("sha256").toString().length()==64);
    assert(json.value("b").toObject().value("sha256").toString().length()==64);
    // A different instance cannot silently overwrite an old matching frame.
    anvil::ReviewPairCapture again(tmp.path().toStdString(), {48});
    assert(!again.save(m,raw,16,16,fsr,32,32,&error));
    std::cout << "ANVIL matched capture contract passed\n";
}
