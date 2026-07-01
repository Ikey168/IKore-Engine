// Test for the Phase 4 symbol-recognition maturity layer - Milestone 16/17, #240.
//
// Verifies the acceptance criteria: a reproducible (deterministic) training-data
// pipeline, a trained model whose holdout accuracy clears a minimum gate, model
// export/import, a confusion-matrix harness that surfaces weak symbols, and a
// heuristic fallback used when no model is available.
//
// Pure std + header-only:
//   g++ -std=c++17 -I src tests/test_symbol_recognizer.cpp -o test_symbol_recognizer

#include "core/sim/Simulation.h"
#include "cv/Vectorize.h"
#include "ml/GlyphNet.h"
#include "ml/SymbolRecognizer.h"

#include <cstdio>
#include <string>
#include <vector>

using namespace IKore;
using namespace IKore::ml;

static int g_failures = 0;

#define CHECK(cond)                                                \
    do {                                                           \
        if (!(cond)) {                                             \
            std::printf("FAIL (line %d): %s\n", __LINE__, #cond);  \
            ++g_failures;                                          \
        }                                                          \
    } while (0)

// The minimum holdout accuracy the trained model must clear (the enforced gate).
static const double kAccuracyGate = 0.90;

static void testDatasetDeterminism() {
    // Same seed + count -> byte-identical dataset (reproducible test path).
    const Dataset a = makeDataset(42, 6);
    const Dataset b = makeDataset(42, 6);
    CHECK(a.X == b.X);
    CHECK(a.y == b.y);
    CHECK(a.size() == 6 * symbolCount());
    CHECK(!a.X.empty() && a.X[0].size() == 12u * 12u);
    // Each class is represented equally.
    std::vector<int> perClass(static_cast<std::size_t>(symbolCount()), 0);
    for (int c : a.y) ++perClass[static_cast<std::size_t>(c)];
    for (int n : perClass) CHECK(n == 6);
    // A different seed yields different data.
    const Dataset c = makeDataset(43, 6);
    CHECK(c.X != a.X);
}

static void testConfusionMatrixMath() {
    // Hand-built matrix so the recall/precision/accuracy math is checked directly,
    // independent of any classifier.
    ConfusionMatrix cm(std::vector<std::string>{"a", "b", "c"});
    cm.add(0, 0); cm.add(0, 0); cm.add(0, 0); cm.add(0, 1); // a: 3 right, 1 -> b
    cm.add(1, 1); cm.add(1, 1);                             // b: 2 right
    cm.add(2, 2); cm.add(2, 0);                             // c: 1 right, 1 -> a

    CHECK(cm.total() == 8);
    CHECK(cm.correct() == 6);
    CHECK(std::fabs(cm.accuracy() - 0.75) < 1e-9);
    CHECK(std::fabs(cm.recall(0) - 0.75) < 1e-9);
    CHECK(std::fabs(cm.recall(1) - 1.0) < 1e-9);
    CHECK(std::fabs(cm.recall(2) - 0.5) < 1e-9);
    CHECK(std::fabs(cm.precision(0) - 0.75) < 1e-9); // predicted a: 3 of 4 correct
    int worst = -1;
    CHECK(std::fabs(cm.minRecall(&worst) - 0.5) < 1e-9);
    CHECK(worst == 2); // class "c" is the weakest, and reporting makes it visible
    const std::string rep = cm.report();
    CHECK(rep.find("c:") != std::string::npos);
    CHECK(rep.find("recall=") != std::string::npos);
}

static void testModelAccuracyGate() {
    const Dataset train = makeDataset(1, 70);
    const Dataset holdout = makeDataset(90001, 20); // disjoint seed -> unseen glyphs
    GlyphClassifier clf;
    clf.train(train.X, train.y, train.labels, 500, 0.5f, 7);

    const ConfusionMatrix cm = evaluate(clf, holdout);
    std::printf("[info] model holdout accuracy %.3f (%d/%d), min per-class recall %.2f\n",
                cm.accuracy(), cm.correct(), cm.total(), cm.minRecall());
    std::printf("%s", cm.report().c_str());

    CHECK(cm.total() == holdout.size());
    CHECK(cm.total() == 20 * symbolCount());
    CHECK(cm.correct() <= cm.total());
    CHECK(cm.accuracy() >= kAccuracyGate); // the enforced minimum-accuracy gate
    CHECK(cm.minRecall() >= 0.70);         // no symbol is catastrophically weak
}

static void testModelBeatsHeuristic() {
    const Dataset train = makeDataset(1, 70);
    const Dataset holdout = makeDataset(90001, 20);
    GlyphClassifier clf;
    clf.train(train.X, train.y, train.labels, 500, 0.5f, 7);

    HeuristicClassifier heur;
    heur.fitCanonical();
    const ConfusionMatrix modelCm = evaluate(clf, holdout);
    const ConfusionMatrix heurCm = evaluateHeuristic(heur, holdout);
    std::printf("[info] heuristic holdout accuracy %.3f (weakest recall %.2f)\n",
                heurCm.accuracy(), heurCm.minRecall());

    // The trained model has "graduated" past the heuristic, but the heuristic is
    // still a usable offline fallback.
    CHECK(modelCm.accuracy() >= heurCm.accuracy());
    CHECK(heurCm.accuracy() >= 0.70);
}

static void testExportRoundTrip() {
    const Dataset train = makeDataset(1, 60);
    const Dataset holdout = makeDataset(90001, 15);
    GlyphClassifier clf;
    clf.train(train.X, train.y, train.labels, 400, 0.5f, 7);

    // Export to a portable blob and reload into a fresh classifier.
    const std::string blob = clf.serialize();
    CHECK(!blob.empty());
    GlyphClassifier reloaded;
    CHECK(reloaded.deserialize(blob));
    CHECK(reloaded.numClasses() == clf.numClasses());
    CHECK(reloaded.featureDim() == clf.featureDim());
    CHECK(reloaded.labels() == clf.labels());

    // The reloaded model predicts identically to the original.
    int mismatches = 0;
    for (int i = 0; i < holdout.size(); ++i) {
        const int a = clf.classify(holdout.X[static_cast<std::size_t>(i)], 0.0f).index;
        const int b = reloaded.classify(holdout.X[static_cast<std::size_t>(i)], 0.0f).index;
        if (a != b) ++mismatches;
    }
    CHECK(mismatches == 0);

    // A malformed blob is rejected, leaving the classifier unusable (falls back).
    GlyphClassifier bad;
    CHECK(!bad.deserialize("not a real model"));
    CHECK(bad.numClasses() == 0);
}

static void testRecognizerFallback() {
    const Dataset train = makeDataset(1, 60);
    GlyphClassifier clf;
    clf.train(train.X, train.y, train.labels, 400, 0.5f, 7);

    // A clean, low-wobble coin glyph both paths should classify confidently.
    sim::DeterministicRng rng(12345);
    RenderParams p = sampleParams(48, rng);
    p.wobble = 0.0f;
    p.rot = 0.0f;
    const cv::Mask coin = renderSymbol(Symbol::Coin, 48, p, rng);
    const int coinIdx = static_cast<int>(Symbol::Coin);

    // No model installed: the recognizer must still produce a guess via the
    // heuristic fallback.
    SymbolRecognizer fallbackOnly;
    CHECK(!fallbackOnly.hasModel());
    const SymbolRecognizer::Prediction fb = fallbackOnly.recognize(coin, 12, 0.3f);
    CHECK(fb.usedFallback);
    CHECK(fb.index == coinIdx);

    // With a model: the recognizer uses the model on a confident glyph.
    SymbolRecognizer withModel;
    withModel.setModel(clf);
    CHECK(withModel.hasModel());
    const SymbolRecognizer::Prediction mp = withModel.recognize(coin, 12, 0.3f);
    CHECK(!mp.usedFallback);
    CHECK(mp.index == coinIdx);

    // Loading a garbage model leaves the recognizer on the fallback path.
    SymbolRecognizer loaded;
    CHECK(!loaded.loadModel("garbage blob"));
    CHECK(!loaded.hasModel());
    const SymbolRecognizer::Prediction lp = loaded.recognize(coin, 12, 0.3f);
    CHECK(lp.usedFallback);

    // Loading a real exported model activates the model path.
    CHECK(loaded.loadModel(clf.serialize()));
    CHECK(loaded.hasModel());
}

int main() {
    testDatasetDeterminism();
    testConfusionMatrixMath();
    testModelAccuracyGate();
    testModelBeatsHeuristic();
    testExportRoundTrip();
    testRecognizerFallback();

    if (g_failures == 0) {
        std::printf("All symbol recognizer tests passed.\n");
        return 0;
    }
    std::printf("%d symbol recognizer test(s) failed.\n", g_failures);
    return 1;
}
