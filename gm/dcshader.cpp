
/*
 * Copyright 2014 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gm.h"
#if SK_SUPPORT_GPU
#include "GrFragmentProcessor.h"
#include "GrCoordTransform.h"
#include "gl/GrGLProcessor.h"
#include "gl/builders/GrGLProgramBuilder.h"
#include "Resources.h"
#include "SkReadBuffer.h"
#include "SkShader.h"
#include "SkStream.h"
#include "SkTypeface.h"
#include "SkWriteBuffer.h"

namespace skiagm {

///////////////////////////////////////////////////////////////////////////////

class DCShader : public SkShader {
public:
    DCShader(const SkMatrix& matrix) : fDeviceMatrix(matrix) {}

    SK_DECLARE_PUBLIC_FLATTENABLE_DESERIALIZATION_PROCS(DCShader);

    void flatten(SkWriteBuffer& buf) const SK_OVERRIDE {
        buf.writeMatrix(fDeviceMatrix);
    }

    bool asFragmentProcessor(GrContext*, const SkPaint& paint, const SkMatrix& viewM,
                             const SkMatrix* localMatrix, GrColor* color,
                             GrFragmentProcessor** fp) const SK_OVERRIDE;
private:
    const SkMatrix fDeviceMatrix;
};

SkFlattenable* DCShader::CreateProc(SkReadBuffer& buf) {
    SkMatrix matrix;
    buf.readMatrix(&matrix);
    return SkNEW_ARGS(DCShader, (matrix));
}

class DCFP : public GrFragmentProcessor {
public:
    DCFP(const SkMatrix& m) : fDeviceTransform(kDevice_GrCoordSet, m) {
        this->addCoordTransform(&fDeviceTransform);
        this->initClassID<DCFP>();
    }

    void getGLProcessorKey(const GrGLCaps& caps,
                            GrProcessorKeyBuilder* b) const SK_OVERRIDE {}

    GrGLFragmentProcessor* createGLInstance() const SK_OVERRIDE {
        class DCGLFP : public GrGLFragmentProcessor {
            void emitCode(GrGLFPBuilder* builder,
                            const GrFragmentProcessor& fp,
                            const char* outputColor,
                            const char* inputColor,
                            const TransformedCoordsArray& coords,
                            const TextureSamplerArray& samplers) {
                GrGLFPFragmentBuilder* fpb = builder->getFragmentShaderBuilder();
                fpb->codeAppendf("vec2 c = %s;", fpb->ensureFSCoords2D(coords, 0).c_str());
                fpb->codeAppend("vec2 r = mod(c, vec2(20.0));");
                fpb->codeAppend("vec4 color = vec4(0.5*sin(c.x / 15.0) + 0.5,"
                                                    "0.5*cos((c.x + c.y) / 15.0) + 0.5,"
                                                    "(r.x + r.y) / 20.0,"
                                                    "distance(r, vec2(15.0)) / 20.0 + 0.2);");
                fpb->codeAppendf("color.rgb *= color.a;"
                                    "%s = color * %s;",
                                    outputColor, GrGLSLExpr4(inputColor).c_str());
            }
            void setData(const GrGLProgramDataManager&, const GrProcessor&) SK_OVERRIDE {}
        };
        return SkNEW(DCGLFP);
    }

    const char* name() const SK_OVERRIDE { return "DCFP"; }

    void onComputeInvariantOutput(GrInvariantOutput* inout) const SK_OVERRIDE {
        inout->mulByUnknownFourComponents();
    }

private:
    bool onIsEqual(const GrFragmentProcessor&) const SK_OVERRIDE { return true; }

    GrCoordTransform fDeviceTransform;
};

bool DCShader::asFragmentProcessor(GrContext*, const SkPaint& paint, const SkMatrix& viewM,
                                   const SkMatrix* localMatrix, GrColor* color,
                                   GrFragmentProcessor** fp) const {
    *fp = SkNEW_ARGS(DCFP, (fDeviceMatrix));
    *color = GrColorPackA4(paint.getAlpha());
    return true;
}

class DCShaderGM : public GM {
public:
    DCShaderGM() {
        this->setBGColor(0xFFAABBCC);
    }

    ~DCShaderGM() SK_OVERRIDE {
        for (int i = 0; i < fPrims.count(); ++i) {
            SkDELETE(fPrims[i]);
        }
    }

protected:

    SkString onShortName() SK_OVERRIDE {
        return SkString("dcshader");
    }

    SkISize onISize() SK_OVERRIDE { return SkISize::Make(1000, 900); }

    void onOnceBeforeDraw() SK_OVERRIDE {
        struct Rect : public Prim {
            SkRect draw(SkCanvas* canvas, const SkPaint& paint) SK_OVERRIDE {
                SkRect rect = SkRect::MakeXYWH(0, 0, 50, 50);
                canvas->drawRect(rect, paint);
                return rect;
            }
        };

        struct Circle : public Prim {
            SkRect draw(SkCanvas* canvas, const SkPaint& paint) SK_OVERRIDE {
                static const SkScalar radius = 25;
                canvas->drawCircle(radius, radius, radius, paint);
                return SkRect::MakeXYWH(0, 0, 2 * radius, 2 * radius);
            }
        };

        struct RRect : public Prim {
            SkRect draw(SkCanvas* canvas, const SkPaint& paint) SK_OVERRIDE {
                SkRRect rrect;
                rrect.setRectXY(SkRect::MakeXYWH(0, 0, 50, 50), 10, 10);
                canvas->drawRRect(rrect, paint);
                return rrect.getBounds();
            }
        };

        struct DRRect : public Prim {
            SkRect draw(SkCanvas* canvas, const SkPaint& paint) SK_OVERRIDE {
                SkRRect outerRRect;
                outerRRect.setRectXY(SkRect::MakeXYWH(0, 0, 50, 50), 5, 5);
                SkRRect innerRRect;
                innerRRect.setRectXY(SkRect::MakeXYWH(5, 8, 35, 30), 8, 3);
                canvas->drawDRRect(outerRRect, innerRRect, paint);
                return outerRRect.getBounds();
            }
        };
        struct Path : public Prim {
            SkRect draw(SkCanvas* canvas, const SkPaint& paint) SK_OVERRIDE {
                SkPath path;
                path.addCircle(15, 15, 10);
                path.addOval(SkRect::MakeXYWH(2, 2, 22, 37));
                path.setFillType(SkPath::kEvenOdd_FillType);
                canvas->drawPath(path, paint);
                return path.getBounds();
            }
        };

        struct Points : public Prim {
            Points(SkCanvas::PointMode mode) : fMode(mode) {}

            SkRect draw(SkCanvas* canvas, const SkPaint& paint) SK_OVERRIDE {
                SkRandom random;
                SkPoint points[500];
                SkRect bounds = SkRect::MakeWH(50, 50);
                int count = SkToInt(SK_ARRAY_COUNT(points));
                if (SkCanvas::kPoints_PointMode != fMode) {
                    count = SkTMin(count, 10);
                }
                for (int p = 0; p < count; ++p) {
                    points[p].fX = random.nextUScalar1() * bounds.width();
                    points[p].fY = random.nextUScalar1() * bounds.width();
                }
                canvas->drawPoints(fMode, count, points, paint);
                return bounds;
            }
            SkCanvas::PointMode fMode;
        };

        struct Text : public Prim {
            SkRect draw(SkCanvas* canvas, const SkPaint& origPaint) SK_OVERRIDE {
                SkPaint paint = origPaint;
                paint.setTextSize(30.f);
                this->setFont(&paint);
                const char* text = this->text();
                static const SkVector offset = SkVector::Make(10, 10);
                canvas->drawText(text, strlen(text), offset.fX, offset.fY, paint);
                SkRect bounds;
                paint.measureText(text, strlen(text), &bounds);
                bounds.offset(offset);
                return bounds;
            }

            virtual void setFont(SkPaint* paint) {
                sk_tool_utils::set_portable_typeface(paint);
            }

            virtual const char* text() const { return "Hello, Skia!"; }
        };

        struct BmpText : public Text {
           void setFont(SkPaint* paint) SK_OVERRIDE {
               if (!fTypeface) {
                    SkString filename = GetResourcePath("/Funkster.ttf");
                    SkAutoTDelete<SkFILEStream> stream(new SkFILEStream(filename.c_str()));
                    if (!stream->isValid()) {
                        SkDebugf("Could not find Funkster.ttf, please set --resourcePath "
                                 "correctly.\n");
                        return;
                    }
                    fTypeface.reset(SkTypeface::CreateFromStream(stream.detach()));
               }
               paint->setTypeface(fTypeface);
            }

            const char* text() const SK_OVERRIDE { return "Hi, Skia!"; }

            SkAutoTUnref<SkTypeface> fTypeface;
        };
        fPrims.push_back(SkNEW(Rect));
        fPrims.push_back(SkNEW(Circle));
        fPrims.push_back(SkNEW(RRect));
        fPrims.push_back(SkNEW(DRRect));
        fPrims.push_back(SkNEW(Path));
        fPrims.push_back(SkNEW(Points(SkCanvas::kPoints_PointMode)));
        fPrims.push_back(SkNEW(Points(SkCanvas::kLines_PointMode)));
        fPrims.push_back(SkNEW(Points(SkCanvas::kPolygon_PointMode)));
        fPrims.push_back(SkNEW(Text));
        fPrims.push_back(SkNEW(BmpText));
    }

    void onDraw(SkCanvas* canvas) SK_OVERRIDE {
        SkPaint paint;
        SkTArray<SkMatrix> devMats;
        devMats.push_back().reset();
        devMats.push_back().setRotate(45, 500, 500);
        devMats.push_back().setRotate(-30, 200, 200);
        devMats.back().setPerspX(-SK_Scalar1 / 2000);
        devMats.back().setPerspY(SK_Scalar1 / 1000);


        SkTArray<SkMatrix> viewMats;
        viewMats.push_back().setScale(0.75f, 0.75f);
        viewMats.push_back().setRotate(45, 50, 50);
        viewMats.back().postScale(0.5f, 1.1f);

        canvas->translate(10, 20);
        canvas->save();
        SkScalar tx = 0, maxTy = 0;
        static const SkScalar kW = 900;

        for (int aa = 0; aa < 2; ++aa) {
            for (int i = 0; i < fPrims.count(); ++i) {
                for (int j = 0; j < devMats.count(); ++j) {
                    for (int k = 0; k < viewMats.count(); ++k) {
                        paint.setShader(SkNEW_ARGS(DCShader, (devMats[j])))->unref();
                        paint.setAntiAlias(SkToBool(aa));
                        canvas->save();
                        canvas->concat(viewMats[k]);
                        SkRect bounds = fPrims[i]->draw(canvas, paint);
                        canvas->restore();
                        viewMats[k].mapRect(&bounds);
                        // add margins
                        bounds.fRight += 20;
                        bounds.fBottom += 20;
                        canvas->translate(bounds.fRight, 0);
                        tx += bounds.fRight;
                        maxTy = SkTMax(bounds.fBottom, maxTy);
                        if (tx > kW) {
                            tx = 0;
                            canvas->restore();
                            canvas->translate(0, maxTy);
                            canvas->save();
                            maxTy = 0;
                        }
                    }
                }
            }
        }
        canvas->restore();
    }

private:
    struct Prim {
        virtual ~Prim() {}
        virtual SkRect draw(SkCanvas*, const SkPaint&) = 0;
    };

    SkTArray<Prim*> fPrims;

    typedef GM INHERITED;
};

DEF_GM( return SkNEW(DCShaderGM); )
}
#endif