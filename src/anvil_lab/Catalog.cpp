// Catalog.cpp — candidate identity, naming contract, roster vocabulary.
#include "Catalog.hpp"

#include <algorithm>

namespace anvil_lab {

const char* const kBaselineName = "ANVIL baseline";

const std::set<std::string> kRosterStatuses = {
    "tryout", "bench", "minor", "rookie", "starter", "cut",
};

std::string buildCandidateName(const std::vector<std::string>& mods,
                               const std::string& rosterStatus) {
    std::string name = kBaselineName;
    for (const std::string& m : mods) {
        name += " + ";
        name += m;
    }
    if (!rosterStatus.empty()) {
        name += " — ";
        name += rosterStatus;
    }
    return name;
}

namespace {

bool isExternalControl(const JsonValue& r) {
    return r.at("kind").asString() == "external_control";
}

bool isBaselineRecord(const JsonValue& r) {
    return r.at("kind").asString() == "anvil_baseline";
}

bool recordHasStatusSuffix(const std::string& name) {
    // The catalog writes an em dash " — " before the status suffix.
    return name.find(" \xe2\x80\x94 ") != std::string::npos;
}

} // namespace

bool validateCandidateRecord(const JsonValue& record,
                             std::vector<std::string>& errors) {
    const std::string id = record.at("id").asString();
    const std::string name = record.at("display_name").asString();
    const std::string kind = record.at("kind").asString();
    if (id.empty()) errors.push_back("record is missing id");
    if (name.empty()) errors.push_back("record is missing display_name");
    if (kind.empty()) errors.push_back("record is missing kind");
    if (!errors.empty()) return false;

    const std::string status = record.at("roster_status").asString();
    if (!status.empty() && kRosterStatuses.count(status) == 0)
        errors.push_back("unknown roster status '" + status + "'");

    if (isBaselineRecord(record)) {
        if (name != kBaselineName)
            errors.push_back("the canonical baseline must be named exactly '"
                             + std::string(kBaselineName) + "', got '" + name + "'");
        if (!status.empty())
            errors.push_back("the canonical baseline never carries a roster status suffix");
        return errors.empty();
    }

    if (kind == "reference") {
        if (name.find("reference") == std::string::npos
            && name.find("control") == std::string::npos)
            errors.push_back("reference record '" + name
                             + "' must identify itself as a reference");
        if (!status.empty())
            errors.push_back("references do not carry roster statuses");
        return errors.empty();
    }

    if (kind == "control") {
        // Experiment control arm (e.g. decoded-frame control): produced by
        // the ANVIL rig but NOT the reconstruction pipeline; it must not
        // claim the baseline lineage or a roster status.
        if (name.find(kBaselineName) != std::string::npos)
            errors.push_back("control '" + name
                             + "' must not claim the ANVIL baseline lineage");
        if (!status.empty())
            errors.push_back("controls do not carry roster statuses");
        if (name.find("control") == std::string::npos)
            errors.push_back("control record '" + name + "' must be labeled a control");
        return errors.empty();
    }

    if (kind == "delivery_variant") {
        if (record.at("parent").asString().empty())
            errors.push_back("delivery variant must name its parent arm");
        const JsonValue& delivery = record.at("delivery");
        const double factor = delivery.at("factor").asNumber(1.0);
        const int64_t inW = record.at("input").at("width").asInt(-1);
        const int64_t outW = record.at("output").at("width").asInt(-1);
        if (inW > 0 && outW > 0 && factor > 0
            && std::abs(factor - static_cast<double>(outW) / static_cast<double>(inW)) > 1e-9)
            errors.push_back("delivery factor does not match recorded geometry");
        if (!status.empty())
            errors.push_back("delivery variants inherit their parent's disposition; "
                             "they do not carry independent roster statuses");
        return errors.empty();
    }

    if (isExternalControl(record)) {
        if (name.find(kBaselineName) == 0)
            errors.push_back("external control '" + name
                             + "' must not claim the ANVIL baseline lineage prefix");
        if (!status.empty())
            errors.push_back("external controls are not ANVIL-derived candidates and "
                             "do not carry Spring Training roster statuses");
        return errors.empty();
    }

    if (kind != "anvil_candidate") {
        errors.push_back("unknown candidate kind '" + kind + "'");
        return false;
    }

    // ANVIL-derived candidate: lineage prefix + at least one truthful
    // modification + status suffix from the vocabulary.
    if (name.find(std::string(kBaselineName) + " + ") != 0)
        errors.push_back("ANVIL candidate '" + name + "' must start with '"
                         + kBaselineName + " + '");
    if (!record.at("modifications").isArray() || record.at("modifications").arr.empty())
        errors.push_back("ANVIL candidate '" + name
                         + "' must list at least one modification vs the baseline");
    if (status.empty())
        errors.push_back("ANVIL candidate '" + name + "' must carry a roster status");
    else if (!recordHasStatusSuffix(name))
        errors.push_back("ANVIL candidate '" + name
                         + "' must end with ' — <status>'");

    // Scale wording: an output wider than the input is upscaled; equal
    // geometry is temporal reconstruction and must not say "upscaled".
    const int64_t inW = record.at("input").at("width").asInt(-1);
    const int64_t outW = record.at("output").at("width").asInt(-1);
    const bool claimsUpscale =
        name.find("upscaled") != std::string::npos
        || record.at("description").asString().find("upscaled") != std::string::npos;
    if (inW > 0 && outW > 0) {
        if (claimsUpscale && outW <= inW)
            errors.push_back("same-resolution output must not be described as upscaled "
                             "(temporal reconstruction at native resolution)");
        if (outW > inW && record.at("scale").at("factor").asNumber(1.0)
                != static_cast<double>(outW) / static_cast<double>(inW))
            errors.push_back("declared scale factor does not match recorded geometry");
    }

    // Replacement vs addition: a modification that marks a stage replaced
    // must name what it replaced; one that marks it added must not.
    for (const JsonValue& mod : record.at("modifications").arr) {
        const std::string change = mod.at("change").asString();
        if (change == "replaced"
            && mod.at("replaces").asString().empty())
            errors.push_back("replaced-stage modification must name the replaced "
                             "component (never imply a combination)");
        if (change == "added" && !mod.at("replaces").asString().empty())
            errors.push_back("added-stage modification must not claim a replacement");
    }
    return errors.empty();
}

bool rosterSetStatus(const std::string& rosterPath, const JsonValue& transition,
                     std::string& err) {
    JsonValue roster;
    if (!jsonReadFile(rosterPath, roster, err)) {
        if (err.rfind("cannot open", 0) == 0) {
            roster = JsonValue::makeObject();
            roster.set("entries", JsonValue::makeObject());
            roster.set("history", JsonValue::makeArray());
        } else {
            return false;
        }
    }
    if (!roster.has("entries") || !roster.at("entries").isObject())
        roster.set("entries", JsonValue::makeObject());
    if (!roster.has("history") || !roster.at("history").isArray())
        roster.set("history", JsonValue::makeArray());

    const std::string id = transition.at("candidate_id").asString();
    const std::string to = transition.at("to").asString();
    const std::string reason = transition.at("reason").asString();
    const std::string authority = transition.at("authority").asString();
    const std::string evidence = transition.at("evidence_ref").asString();
    if (id.empty() || to.empty() || reason.empty() || authority.empty()
        || evidence.empty()) {
        err = "transition requires candidate_id, to, reason, authority, evidence_ref";
        return false;
    }
    if (kRosterStatuses.count(to) == 0) {
        err = "unknown roster status '" + to + "'";
        return false;
    }
    if (id == kBaselineName || id == "baseline") {
        err = "the canonical baseline never carries a roster status";
        return false;
    }
    // One disposition per technical configuration across ALL scenes.
    // Scene/run IDs are observations, not roster identity.
    if (id.rfind("cfg:", 0) != 0 || id.size() != 68) {
        err = "candidate_id must be cfg:<64-hex canonical config hash>, not a scene/run ID";
        return false;
    }
    for (size_t i = 4; i < id.size(); ++i)
        if (!((id[i] >= '0' && id[i] <= '9') ||
              (id[i] >= 'a' && id[i] <= 'f'))) {
            err = "candidate_id contains invalid configuration digest";
            return false;
        }
    if (to == "starter") {
        // A caller-controlled boolean, commit string, and authority name are
        // NOT independently authenticated merge/maintainer evidence. This
        // local roster API does not possess a trusted GitHub authorization
        // channel. Fail closed rather than falsely announcing a starter.
        // A future authenticated promotion path must verify BOTH an actual
        // merged commit on main and the authorized maintainer decision.
        err = "starter promotion blocked: this local roster writer cannot "
              "verify GitHub main merge ancestry and authenticated maintainer "
              "authorization; self-declared integration_commit, merge_evidence "
              "or authority fields are NOT proof. Keep rookie pending "
              "independent authenticated integration.";
        return false;
    }
    std::string from;
    {
        const JsonValue& entry = roster.at("entries").at(id);
        if (entry.isObject()) from = entry.at("status").asString();
    }
    if (from == to) {
        err = "candidate '" + id + "' already has status '" + to + "'";
        return false;
    }
    if (!from.empty() && kRosterStatuses.count(from) == 0) {
        err = "roster file corrupt: unknown current status '" + from + "'";
        return false;
    }

    // Entries/history are copied out (at() is const), mutated, then set back.
    JsonValue entries = roster.at("entries").isObject()
        ? roster.at("entries") : JsonValue::makeObject();
    JsonValue newEntry = JsonValue::makeObject();
    newEntry.set("status", JsonValue::makeString(to));
    bool hadEntry = false;
    for (auto& kv : entries.obj) {
        if (kv.first == id) {
            kv.second = newEntry;
            hadEntry = true;
            break;
        }
    }
    if (!hadEntry) entries.obj.emplace_back(id, std::move(newEntry));
    roster.set("entries", std::move(entries));

    JsonValue history = roster.at("history").isArray()
        ? roster.at("history") : JsonValue::makeArray();
    JsonValue h = JsonValue::makeObject();
    h.set("candidate_id", JsonValue::makeString(id));
    h.set("from", JsonValue::makeString(from));
    h.set("to", JsonValue::makeString(to));
    h.set("timestamp", JsonValue::makeString(transition.at("timestamp").asString()));
    h.set("reason", JsonValue::makeString(reason));
    h.set("authority", JsonValue::makeString(authority));
    h.set("evidence_ref", JsonValue::makeString(evidence));
    if (!transition.at("integration_commit").asString().empty())
        h.set("integration_commit",
              JsonValue::makeString(transition.at("integration_commit").asString()));
    history.arr.push_back(std::move(h));
    roster.set("history", std::move(history));
    return jsonWriteFile(rosterPath, roster, err);
}

bool rosterAudit(const JsonValue& roster, const JsonValue& catalog,
                 std::vector<std::string>& errors) {
    std::set<std::string> known;
    if (catalog.isArray())
        for (const JsonValue& r : catalog.arr) {
            // Historic scene-specific experiment IDs remain valid for old
            // decision history, but current dispositions use candidate_key.
            known.insert(r.at("id").asString());
            if (r.at("kind").asString() == "anvil_candidate" &&
                !r.at("candidate_key").asString().empty())
                known.insert(r.at("candidate_key").asString());
        }
    for (const auto& kv : roster.at("entries").obj) {
        if (kv.first.rfind("cfg:", 0) != 0)
            errors.push_back("current roster entry is scene-scoped, not global: '" + kv.first + "'");
        if (!known.count(kv.first))
            errors.push_back("roster references unknown candidate '" + kv.first + "'");
        const std::string st = kv.second.at("status").asString();
        if (kRosterStatuses.count(st) == 0)
            errors.push_back("roster status '" + st + "' not in the vocabulary");
        if (st == "starter")
            errors.push_back("starter roster entry requires external authenticated "
                "verification; this local writer/auditor cannot certify it");
    }
    std::set<std::string> seenFrom;
    for (const JsonValue& h : roster.at("history").arr) {
        // History is append-only evidence: entries never mutate records, so
        // the audit only checks referential integrity and starter evidence.
        if (!known.count(h.at("candidate_id").asString()))
            errors.push_back("history references unknown candidate '"
                             + h.at("candidate_id").asString() + "'");
        if (h.at("to").asString() == "starter")
            errors.push_back("starter history cannot be certified by local "
                "self-asserted evidence; independently verify merge on main "
                "and authenticated maintainer approval before qualification");
    }
    return errors.empty();
}

JsonValue pipelineDiff(const JsonValue& candidate, const JsonValue& baseline) {
    JsonValue out = JsonValue::makeObject();
    JsonValue changed = JsonValue::makeArray();
    JsonValue unchanged = JsonValue::makeArray();
    const JsonValue& cStages = candidate.at("pipeline").at("stages");
    const JsonValue& bStages = baseline.at("pipeline").at("stages");

    auto stageMap = [](const JsonValue& stages) {
        std::vector<std::pair<std::string, const JsonValue*>> m;
        for (const JsonValue& s : stages.arr)
            m.emplace_back(s.at("name").asString(), &s);
        return m;
    };
    const auto cMap = stageMap(cStages);
    const auto bMap = stageMap(bStages);
    for (const auto& [name, cs] : cMap) {
        const JsonValue* bs = nullptr;
        for (const auto& [bName, bPtr] : bMap)
            if (bName == name) { bs = bPtr; break; }
        const std::string cMode = cs->at("mode").asString();
        const bool cActive = cs->at("active").asBool(false);
        if (!bs) {
            JsonValue e = JsonValue::makeObject();
            e.set("stage", JsonValue::makeString(name));
            e.set("change", JsonValue::makeString("added"));
            e.set("detail", JsonValue::makeString(
                "stage present here and absent in the reference configuration"));
            changed.arr.push_back(std::move(e));
            continue;
        }
        const std::string bMode = bs->at("mode").asString();
        const bool bActive = bs->at("active").asBool(false);
        if (cMode == bMode && cActive == bActive) {
            unchanged.arr.push_back(JsonValue::makeString(name));
        } else {
            JsonValue e = JsonValue::makeObject();
            e.set("stage", JsonValue::makeString(name));
            if (!cActive && bActive) e.set("change", JsonValue::makeString("disabled"));
            else e.set("change", JsonValue::makeString("parameter"));
            e.set("detail", JsonValue::makeString(
                "reference: " + bMode + (bActive ? "" : " (inactive)")
                + " → this configuration: " + cMode + (cActive ? "" : " (inactive)")));
            changed.arr.push_back(std::move(e));
        }
    }
    for (const auto& [name, bs] : bMap) {
        bool found = false;
        for (const auto& [cName, cPtr] : cMap)
            if (cName == name) { found = true; break; }
        if (found) continue;
        JsonValue e = JsonValue::makeObject();
        e.set("stage", JsonValue::makeString(name));
        e.set("change", JsonValue::makeString("removed_or_disabled"));
        e.set("detail", JsonValue::makeString(
            "stage present in the reference configuration and absent here"));
        changed.arr.push_back(std::move(e));
    }
    // Delivery scaling is an explicit comparison variable, not a pipeline
    // stage; surface it so a changed delivery scale is never buried.
    const double cScale = candidate.at("scale").at("factor").asNumber(1.0);
    const double bScale = baseline.at("scale").at("factor").asNumber(1.0);
    if (cScale != bScale) {
        JsonValue e = JsonValue::makeObject();
        e.set("stage", JsonValue::makeString("delivery_scale"));
        e.set("change", JsonValue::makeString("parameter"));
        e.set("detail", JsonValue::makeString("delivery scale "
            + std::to_string(bScale) + "x → " + std::to_string(cScale) + "x"));
        changed.arr.push_back(std::move(e));
    }
    out.set("changed", std::move(changed));
    out.set("unchanged", std::move(unchanged));
    return out;
}

} // namespace anvil_lab
