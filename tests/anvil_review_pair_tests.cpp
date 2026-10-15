// Contract-level test of atomic same-frame capture and fail-closed pairing.
#include "core/ReviewPairCapture.hpp"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QTemporaryDir>
#include <cstdlib>
#define REQUIRE(cond) do { if (!(cond)) { \
    std::cerr << "CHECK FAILED: " << #cond << " at " << __LINE__ << "\\n"; \
    return EXIT_FAILURE; } } while (false)
#include <iostream>
#include <vector>

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir tmp;
    REQUIRE(tmp.isValid());
    const QByteArray key(32, 'K');
    anvil::ReviewPairCapture recorder(tmp.path().toStdString(), {48}, key);
    REQUIRE(recorder.enabled() && recorder.wants(48) && !recorder.wants(49));
    anvil::ReviewFrameMeta m;
    m.frameIndex=48; m.ptsUs=1'600'000; m.ptsTicks=48;
    m.tbNum=1; m.tbDen=30; m.sourceW=16; m.sourceH=16;
    m.anvilStageMs=2.3; m.fsrDispatchMs=4.1;
    m.sourceId="fixture.mov"; m.gitSha=std::string(40,'a');
    m.gitDirty="true"; m.gitDirtyHash=std::string(64,'b');
    m.provenanceSource="synthetic-test-fixture";
    m.binarySha256=std::string(64,'c'); m.inputSha256=std::string(64,'d');
    std::vector<uint8_t> raw(16*16*4, 255), fsr(32*32*4, 255);
    for(size_t i=0;i<raw.size();i+=4) {
        raw[i]=static_cast<uint8_t>((i/4)%127);
        raw[i+1]=73; raw[i+2]=149;
    }
    QString error;
    REQUIRE(!recorder.save(m,raw,16,16,fsr,32,31,&error));
    REQUIRE(recorder.wants(48));
    REQUIRE(!recorder.save(m,raw,16,18,fsr,32,32,&error));
    REQUIRE(recorder.save(m,raw,16,16,fsr,32,32,&error));
    REQUIRE(!recorder.wants(48));
    REQUIRE(!recorder.save(m,raw,16,16,fsr,32,32,&error));
    const QString path = QDir(tmp.path()).filePath("frame_000048");
    const QImage imageA(QDir(path).filePath("anvil_causal_live_control.png"));
    const QImage imageB(QDir(path).filePath("anvil_causal_live_fsr41.png"));
    REQUIRE(imageA.width()==32 && imageA.height()==32);
    REQUIRE(imageB.width()==32 && imageB.height()==32);
    QFile file(QDir(path).filePath("manifest.json"));
    REQUIRE(file.open(QIODevice::ReadOnly));
    const QJsonObject json = QJsonDocument::fromJson(file.readAll()).object();
    REQUIRE(json.value("schema").toString()=="anvil-visual-pair-v2");
    REQUIRE(json.value("git_dirty").isBool() && json.value("git_dirty").toBool());
    REQUIRE(json.value("git_dirty_hash").toString()==QString::fromStdString(m.gitDirtyHash));
    REQUIRE(json.value("binary_sha256").toString()==QString::fromStdString(m.binarySha256));
    REQUIRE(json.value("input_sha256").toString()==QString::fromStdString(m.inputSha256));
    REQUIRE(json.value("capture_attestation").toString()=="producer-hmac-sha256-v1");
    REQUIRE(QFile::exists(QDir(path).filePath("capture.hmac")));
    REQUIRE(json.value("verified_same_frame").toBool());
    REQUIRE(json.value("pts_ticks").toInteger()==48);
    REQUIRE(json.value("a").toObject().value("status").toString()=="control");
    REQUIRE(json.value("b").toObject().value("status").toString()=="tryout");
    REQUIRE(json.value("a").toObject().value("sha256").toString().length()==64);
    REQUIRE(json.value("b").toObject().value("sha256").toString().length()==64);
    // A different instance cannot silently overwrite an old matching frame.
    anvil::ReviewPairCapture again(tmp.path().toStdString(), {48}, key);
    REQUIRE(!again.save(m,raw,16,16,fsr,32,32,&error));
    std::cout << "ANVIL matched capture contract passed\n";
}
