// ****************************************************************************
// PixInsight Class Library - PCL 02.00.14.0695
// Standard CometAlignment Process Module Version 01.02.06.0070
// ****************************************************************************
// CometAlignmentInstance.cpp - Released 2015/03/04 19:50:08 UTC
// ****************************************************************************
// This file is part of the standard CometAlignment PixInsight module.
//
// Copyright (c) 2012-2015 Nikolay Volkov
// Copyright (c) 2003-2015 Pleiades Astrophoto S.L.
//
// Redistribution and use in both source and binary forms, with or without
// modification, is permitted provided that the following conditions are met:
//
// 1. All redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//
// 2. All redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// 3. Neither the names "PixInsight" and "Pleiades Astrophoto", nor the names
//    of their contributors, may be used to endorse or promote products derived
//    from this software without specific prior written permission. For written
//    permission, please contact info@pixinsight.com.
//
// 4. All products derived from this software, in any form whatsoever, must
//    reproduce the following acknowledgment in the end-user documentation
//    and/or other materials provided with the product:
//
//    "This product is based on software from the PixInsight project, developed
//    by Pleiades Astrophoto and its contributors (http://pixinsight.com/)."
//
//    Alternatively, if that is where third-party acknowledgments normally
//    appear, this acknowledgment must be reproduced in the product itself.
//
// THIS SOFTWARE IS PROVIDED BY PLEIADES ASTROPHOTO AND ITS CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
// TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL PLEIADES ASTROPHOTO OR ITS
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, BUSINESS
// INTERRUPTION; PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; AND LOSS OF USE,
// DATA OR PROFITS) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
// ****************************************************************************

#include "CometAlignmentInstance.h"
#include "CometAlignmentInterface.h"
#include "CometAlignmentModule.h" // for ReadableVersion()

#include <pcl/ErrorHandler.h>
#include <pcl/FileFormat.h>
#include <pcl/LinearFit.h>
#include <pcl/ProcessInterface.h> // for ProcessEvents()
#include <pcl/SpinStatus.h>
#include <pcl/StdStatus.h>
#include <pcl/Translation.h>
#include <pcl/Vector.h>
#include <pcl/Version.h>
#include <pcl/DrizzleDataDecoder.h>
#include <pcl/Algebra.h>

namespace pcl
{

static PixelInterpolation* pixelInterpolation = 0;

CometAlignmentInstance::CometAlignmentInstance (const MetaProcess* m) :
ProcessImplementation (m),
p_targetFrames (),
p_inputHints( TheCAInputHintsParameter->DefaultValue() ),
p_outputHints( TheCAOutputHintsParameter->DefaultValue() ),
p_outputDir (TheOutputDir->DefaultValue ()),
p_outputExtension( TheCAOutputExtensionParameter->DefaultValue() ),
p_overwrite (TheOverwrite->DefaultValue ()),
p_prefix (ThePrefix->DefaultValue ()),
p_postfix (ThePostfix->DefaultValue ()),
p_reference (TheReference->DefaultValue ()),
p_subtractFile (TheSubtractFile->DefaultValue ()),
p_subtractMode (TheSubtractMode->DefaultValue ()),
p_OperandIsDI (TheOperandIsDI->DefaultValue ()),
p_normalize (TheNormalize->DefaultValue ()),
p_enableLinearFit (TheEnableLinearFit->DefaultValue ()),
p_rejectLow (TheRejectLow->DefaultValue ()),
p_rejectHigh (TheRejectHigh->DefaultValue ()),
p_drzSaveSA (TheDrzSaveSA->DefaultValue ()),
p_drzSaveCA (TheDrzSaveCA->DefaultValue ()),
p_pixelInterpolation (ThePixelInterpolationParameter->DefaultValueIndex ()),
p_linearClampingThreshold (TheLinearClampingThresholdParameter->DefaultValue ()) { }

CometAlignmentInstance::CometAlignmentInstance (const CometAlignmentInstance& x) :
ProcessImplementation (x)
{
   Assign (x);
}

void CometAlignmentInstance::Assign (const ProcessImplementation& p)
{
   const CometAlignmentInstance* x = dynamic_cast<const CometAlignmentInstance*> (&p);
   if (x != 0)
   {
      p_targetFrames = x->p_targetFrames;
      p_inputHints = x->p_inputHints;
      p_outputHints = x->p_outputHints;
      p_outputDir = x->p_outputDir;
      p_outputExtension = x->p_outputExtension;
      p_overwrite = x->p_overwrite;
      p_prefix = x->p_prefix;
      p_postfix = x->p_postfix;
      p_reference = x->p_reference;
      p_subtractFile = x->p_subtractFile;
      p_enableLinearFit = x->p_enableLinearFit;
      p_rejectLow = x->p_rejectLow;
      p_rejectHigh = x->p_rejectHigh;
      p_normalize = x->p_normalize;
	  p_drzSaveSA = x->p_drzSaveSA;
	  p_drzSaveCA = x->p_drzSaveCA;
      p_subtractMode = x->p_subtractMode;
	  p_OperandIsDI = x->p_OperandIsDI,
      p_pixelInterpolation = x->p_pixelInterpolation;
      p_linearClampingThreshold = x->p_linearClampingThreshold;
   }
}

bool CometAlignmentInstance::CanExecuteOn (const View& view, String& whyNot) const
{
   whyNot = "CometAlignment can only be executed in the global context.";
   return false;
}

bool CometAlignmentInstance::IsHistoryUpdater (const View& view) const
{
   return false;
}

bool CometAlignmentInstance::CanExecuteGlobal (String& whyNot) const
{
   if (p_targetFrames.IsEmpty ())
      whyNot = "No target frames have been specified.";

   else if (!p_outputDir.IsEmpty () && !File::DirectoryExists (p_outputDir))
      whyNot = "The specified output directory does not exist: " + p_outputDir;
   else if (!p_subtractFile.IsEmpty () && !File::Exists (p_subtractFile))
      whyNot = "The specified file for LinearFit does not exist: " + p_subtractFile;

   else
   {
      for (image_list::const_iterator i = p_targetFrames.Begin (); i != p_targetFrames.End (); ++i)
         if (i->enabled && !File::Exists (i->path))
         {
            whyNot = "File not found: " + i->path;
            return false;
         }
      whyNot.Clear ();
      return true;
   }
   return false;
}

// ----------------------------------------------------------------------------

static void ImageVariant2ImageWindow( ImageVariant* v )

{
    if ( v == 0 )

    {
        Console().WriteLn( "Wrong target pointer" );
        return;
    }
    ImageWindow w( v->AnyImage()->Width(), v->AnyImage()->Height(), v->AnyImage()->NumberOfChannels(), v->BitsPerSample(), v->IsFloatSample(), v->AnyImage()->IsColor(), true, "debug" );
    w.MainView().Image().CopyImage( *v );
    w.Show();
}

/*
static void ImageVariant2ImageWindow( ImageVariant v )

{
    ImageWindow w( v.AnyImage()->Width(), v.AnyImage()->Height(), v.AnyImage()->NumberOfChannels(), v.BitsPerSample(), v.IsFloatSample(), v.AnyImage()->IsColor(), true, "debug" );
    w.MainView().Image().CopyImage( v );
    w.Show();

}
 */
/*
// ----------------------------------------------------------------------------
static void DImage2ImageWindow( const DImage* img )

{
    ImageWindow w( img->Width(), img->Height(), img->NumberOfChannels(), img->BitsPerSample(), img->IsFloatSample(), img->IsColor(), true, "debug" );
    w.MainView().Image().CopyImage( *img );
    w.Show();
}

 */

// ----------------------------------------------------------------------------

class LinearFitEngine
{
public:

   typedef GenericVector<LinearFit> linear_fit_set;

   LinearFitEngine (const float _rejectLow, const float _rejectHigh) : rejectLow (_rejectLow), rejectHigh (_rejectHigh) 
   {
   }

   linear_fit_set
   Fit (String& status, const ImageVariant& image, const ImageVariant& reference)
   {
	   status = "LFit calc";
	  #if debug
	  Console().Write("Calc LF ");
	  #endif
      if (!image.IsComplexSample ())
         if (image.IsFloatSample ())
            switch (image.BitsPerSample ())
            {
            case 32: return Fit (static_cast<const Image&> (*image), reference);
               break;
            case 64: return Fit (static_cast<const DImage&> (*image), reference);
               break;
            }
         else
            switch (image.BitsPerSample ())
            {
            case 8: return Fit (static_cast<const UInt8Image&> (*image), reference);
               break;
            case 16: return Fit (static_cast<const UInt16Image&> (*image), reference);
               break;
            case 32: return Fit (static_cast<const UInt32Image&> (*image), reference);
               break;
            }
      return linear_fit_set ();
   }

   void
   Apply (ImageVariant& image, String& status, const linear_fit_set& L)
   {
	   status = "LFit Apply";
	   #if debug
	   Console().Write("Apply LF ");
	   #endif
      if (!image.IsComplexSample ())
         if (image.IsFloatSample ())
            switch (image.BitsPerSample ())
            {
            case 32: Apply (static_cast<Image&> (*image), L);
               break;
            case 64: Apply (static_cast<DImage&> (*image), L);
               break;
            }
         else
            switch (image.BitsPerSample ())
            {
            case 8:
            {
               UInt8Image& wrk = static_cast<UInt8Image&> (*image);
               Image tmp (wrk);
               Apply (tmp, L);
               wrk.Apply (tmp);
            }
               break;
            case 16:
            {
               UInt16Image& wrk = static_cast<UInt16Image&> (*image);
               Image tmp (wrk);
               Apply (tmp, L);
               wrk.Apply (tmp);
            }
               break;
            case 32:
            {
               UInt32Image& wrk = static_cast<UInt32Image&> (*image);
               DImage tmp (wrk);
               Apply (tmp, L);
               wrk.Apply (tmp);
            }
               break;
            }
   }

private:

   const float rejectLow;
   const float rejectHigh;

   template <class P1, class P2>
   linear_fit_set
   Fit (const GenericImage<P1>& image, const GenericImage<P2>& reference)
   {
      linear_fit_set L (image.NumberOfNominalChannels ());
      GenericVector<size_type> count (image.NumberOfNominalChannels ());
      size_type N = image.NumberOfPixels ();

      for (int c = 0; c < image.NumberOfNominalChannels (); ++c)
      {
         Array<float> F1, F2;
         F1.Reserve (N);
         F2.Reserve (N);
         const typename P1::sample* v1 = image.PixelData (c);
         const typename P1::sample* vN = v1 + N;
         const typename P2::sample* v2 = reference.PixelData (c);
         for (; v1 < vN; ++v1, ++v2)
         {
            float f1;
            P1::FromSample (f1, *v1);
            if (f1 > rejectLow && f1 < rejectHigh)
            {
               float f2;
               P2::FromSample (f2, *v2);
               if (f2 > rejectLow && f2 < rejectHigh)
               {
                  F1.Add (f1);
                  F2.Add (f2);
               }
            }
         }

         if (F1.Length () < 3)
            throw Error ("Insufficient data (channel " + String (c) + ')');

         count[c] = F1.Length ();

         L[c] = LinearFit (F1, F2);

         if (!L[c].IsValid ())
            throw Error ("Invalid linear fit (channel " + String (c) + ')');
      }
      return L;
   }

   template <class P>
   linear_fit_set
   Fit (const GenericImage<P>& image, const ImageVariant& reference)
   {
      if (!reference.IsComplexSample ())
         if (reference.IsFloatSample ())
            switch (reference.BitsPerSample ())
            {
            case 32: return Fit (image, static_cast<const Image&> (*reference));
               break;
            case 64: return Fit (image, static_cast<const DImage&> (*reference));
               break;
            }
         else
            switch (reference.BitsPerSample ())
            {
            case 8: return Fit (image, static_cast<const UInt8Image&> (*reference));
               break;
            case 16: return Fit (image, static_cast<const UInt16Image&> (*reference));
               break;
            case 32: return Fit (image, static_cast<const UInt32Image&> (*reference));
               break;
            }
      return linear_fit_set ();
   }

   template <class P>
   void
   Apply (GenericImage<P>& image, const linear_fit_set& L)
   {
      for (int c = 0; c < image.NumberOfNominalChannels (); ++c)
      {
         typename P::sample* v = image.PixelData (c);
         typename P::sample* vN = v + image.NumberOfPixels ();
         for (; v < vN; ++v)

         {
            double f;
            if (*v > 0) //ignore black pixels

            {
               P::FromSample (f, *v);
               *v = P::ToSample (L[c](f));
            }
         }
      }

      image.Truncate ();
   }
};

// ----------------------------------------------------------------------------
Matrix DeltaToMatrix(const DPoint delta)
{	//comet movement matrix
	return Matrix(
		1.0, 0.0, delta.x,
		0.0, 1.0, delta.y,
		0.0, 0.0, 1.0);
}
/*
 * Homography transformation
 */
class Homography
{
public:

   Homography() : H( Matrix::UnitMatrix( 3 ) )
   {
   }

   Homography( const Matrix& aH ) : H( aH )
   {
   }

   Homography( const Homography& h ) : H( h.H )
   {
   }

   template <typename T>
   DPoint operator ()( T x, T y ) const
   {
      double w = H[2][0]*x + H[2][1]*y + H[2][2];
      PCL_CHECK( 1 + w != 1 )
      return DPoint( (H[0][0]*x + H[0][1]*y + H[0][2])/w,
                     (H[1][0]*x + H[1][1]*y + H[1][2])/w );
   }

   template <typename T>
   DPoint operator ()( const GenericPoint<T>& p ) const
   {
      return operator ()( p.x, p.y );
   }

   Homography Inverse() const
   {
      return Homography( H.Inverse() );
   }

   operator const Matrix&() const
   {
      return H;
   }

   bool IsValid() const
   {
      return !H.IsEmpty();
   }

   void SetUnique()
   {
      H.SetUnique();
   }

private:

   Matrix H;
	
};

// ----------------------------------------------------------------------------

struct FileData
{
   FileFormat* format; // the file format of retrieved data
   const void* fsData; // format-specific data
   ImageOptions options; // currently used for resolution only
   FITSKeywordArray keywords; // FITS keywords
   ICCProfile profile; // ICC profile
   ByteArray metadata; // XML metadata

   FileData () :
   format (0), fsData (0), options (), keywords (), profile (), metadata () { }

   FileData (FileFormatInstance& file, const ImageOptions & o) :
   format (0), fsData (0), options (o), keywords (), profile (), metadata ()
   {
      format = new FileFormat (file.Format ());

      if (format->UsesFormatSpecificData ())
         fsData = file.FormatSpecificData ();

      if (format->CanStoreKeywords ())
         file.Extract (keywords);

      if (format->CanStoreICCProfiles ())
         file.Extract (profile);

      if (format->CanStoreMetadata ())
      {

         void* p = 0;
         size_t n = 0;
         file.Extract (p, n);
         if (p != 0)
         {
            metadata = ByteArray (ByteArray::iterator (p), ByteArray::iterator (p) + n);
            delete (uint8*) p;
         }
      }
   }

   ~FileData ()
   {
      if (format != 0)
      {
         if (fsData != 0)
            format->DisposeFormatSpecificData (const_cast<void*> (fsData)), fsData = 0;
         delete format, format = 0;
      }
   }
};

class DrizzleSAFilter : public DrizzleDecoderBase
{
public:

   DrizzleSAFilter() : DrizzleDecoderBase()
   {
      Initialize();
   }

   virtual ~DrizzleSAFilter()
   {
   }

   bool HasSplines() const
   {
      return m_hasSplines;
   }

   bool HasMatrix() const
   {
      return m_hasMatrix;
   }

private:

   bool m_hasSplines : 1;
   bool m_hasMatrix  : 1;
   
   virtual void Initialize()
   {
      m_hasSplines = false;
      m_hasMatrix = false;
   }
	
   // Returns true to filter out a .drz item, false to keep it.
   virtual bool FilterBlock( const IsoString& itemId )
   {
      if ( itemId == "Sx" || itemId == "Sy" )
      {
         m_hasSplines = true;
         return true;
      }

      if ( itemId == "H" )
      {
         m_hasMatrix = true;
         return false;
      }
      return itemId != "P" && itemId != "T" && itemId != "D";
   }
};

class DrizzleSADataDecoder : public DrizzleDataDecoder
{
public:

   DrizzleSADataDecoder() : DrizzleDataDecoder()
   {
   }

   virtual ~DrizzleSADataDecoder()
   {
   }
   
private:

   virtual void Validate()
   {  
		if ( m_filePath.IsEmpty() )
         throw Error( "No file path definition." );
      if ( m_referenceWidth < 1 )
         throw Error( "No reference width definition." );
      if ( m_referenceHeight < 1 )
         throw Error( "No reference height definition." );
      if ( m_H.IsEmpty() )
         throw Error( "No alignment matrix definition." );
   }
};
// ----------------------------------------------------------------------------

class CAThread : public Thread
{
public:

	String monitor;		//curent processing step status
	int monitor2;

   CAThread (ImageVariant* t, FileData* fd, ImageVariant* drzI, FileData* drzD, const String& tp, const String& dp, const DPoint d, const Matrix m, const CometAlignmentInstance* _instance) :
   target (t), fileData (fd), drzImage(drzI), drzData(drzD), targetPath (tp), drzPath (dp), delta (d), drzMatrix(m), operand (_instance->m_OperandImage)
   {
	   drizzle = !drzMatrix.IsEmpty();
	   monitor = "Prepare";
	   monitor2 = 0;
	   i = _instance;
   }

   virtual
   ~CAThread ()
   {
      if (target != 0)
         delete target, target = 0;

      if (fileData != 0)
         delete fileData, fileData = 0;

	  if (drzImage != 0)
         delete drzImage, drzImage = 0;

	  if (drzData != 0)
         delete drzData, drzData = 0;
   }

   virtual void
   Run ()
   {
      try
      {
		  if (TryIsAborted()) 
			  return;

		  Matrix dM(DeltaToMatrix(delta)); //comet movement matrix
		  Matrix cM(DeltaToMatrix(DPoint(0.5,0.5))); //convertion Matrix.
		  if(!operand) 
		  {
			  monitor = "Align Target";
			  HomographyApplyTo(*target, dM); //comet movement matrix
		  }
		  else
		  {	
			  ImageVariant o;
			  o.CopyImage (*operand); //Create local copy of Operand image

			  if (i->p_subtractMode) //move Operand(ComaIntegration) and subtract -> create PureStarAligned
			  {
				  monitor = "Align Operand";
				  Matrix M(dM);
				  if(i->p_OperandIsDI) //Operand is DrizzleIntegration
				  {
					  //convert DrizzleIntegration coordinates to StarAlignment coordinates.
					  M = M * cM; 
					  M /= M[2][2];
				  }
				  M.Invert(); //Invert alignments direction
				  HomographyApplyTo(o, M); //Invert delta to align Operand(CometIntegration) to comet position
				  if (i->p_enableLinearFit)
				  {
					  LinearFitEngine E (i->p_rejectLow, i->p_rejectHigh);
					  LFSet = E.Fit (monitor, o, *target);
					  E.Apply (o, monitor, LFSet); //LinearFit Operand to Target
				  }
				  Normalize (o);				  
				  (*target) -= o; //Subtract Operand(CometIntegration) from StarAligned and create PureStarAligned
			  }	
			  else //subtract Operand(StarIntegration) and move to comet position -> create PureCometAligned 
			  {
				  if(i->p_OperandIsDI) //Operand is DrizzleIntegration
				  { 
					  monitor = "Align DI->SI";
					  //convert Operand DrizzleIntegration coordinates to StarAlignment coordinates.
					  HomographyApplyTo(o, cM.Inverse());
				  }
				  if (i->p_enableLinearFit)
				  {
					  LinearFitEngine E (i->p_rejectLow, i->p_rejectHigh);
					  LFSet = E.Fit (monitor,o, *target);
					  E.Apply (o,monitor, LFSet); //LinearFit Operand to Target
				  }
				  Normalize (o);					  
				  (*target) -= o; //Subtract Operand from Target Image
				  monitor = "Align Target";
				  HomographyApplyTo(*target, dM); //align Result to comet position
			  }
			  (*target).Truncate (); // Truncate to [0,1]

			  if (TryIsAborted()) return;

			  if(drizzle)// Create from NonAligned new PureStarNonAligned or PureComaNonAligned Image. 
			  {
				  o.CopyImage (*operand); //Create local copy of Operand image
				  
				  Matrix M(drzMatrix); 
				  if (i->p_subtractMode) //Mode Checked -> Operand is ComaIntegration 
				  {
					  M = M * dM; // integrate star alignment matrix and comet movement matrix
					  M /= M[2][2];
				  }
				  else  //Mode UnChecked -> Operand is StarIntegration
				  {
					  //M == drzMatrix
				  }
				  
				  if(i->p_OperandIsDI) //Operand is DrizzleIntegration
				  {  //convert DrizzleIntegration coordinates to StarAlignment coordinates.
					  M = M * cM; 
					  M /= M[2][2];
				  }

				  M.Invert(); //Invert alignments direction
				  monitor = "Align Operand";
				  HomographyApplyTo(o, M); //Align Operand to Origin drizle integrable 
				  
				  if (TryIsAborted()) return;
				  
				  if (i->p_enableLinearFit)
				  {
					  LinearFitEngine E(i->p_rejectLow, i->p_rejectHigh);
					  LFSet = E.Fit (monitor, o, *drzImage);
					  E.Apply (o, monitor, LFSet); //LinearFit Operand to Target
				  }
				  if (TryIsAborted()) return;
				  
				  Normalize (o);
				  
				  (*drzImage) -= o; //Subtract Operand from Target Image
				  (*drzImage).Truncate(); // Truncate to [0,1]
				  /*
				  if (TryIsAborted()) return;
				  
				  if(i->p_drzSaveSA) //Optional: create from PureNonAligned the PureStarAligned
				  {
					  saImg.CopyImage(*drzImage);
					  M = drzMatrix; // use star alignment matrix
					  monitor = "Align PureStar";
					  HomographyApplyTo(saImg, M);
				  }

				  if (TryIsAborted()) return;	
				  
				  if(i->p_drzSaveCA) //Optional: create from PureNonAligned the PureCometAligned
				  {
					  monitor = "Align PureComet";
					  caImg.CopyImage(*drzImage);
					  M = drzMatrix*DeltaToMatrix(delta); // integrate star alignment matrix and comet movement matrix
					  M /= M[2][2];
					  HomographyApplyTo(caImg, M); 
				  }
				  */
			  }
		  }
      }
      catch (...)
      {
         /*
          * NB: No exceptions can propagate out of a thread!
          * We *only* throw exceptions to stop this thread when data.abort has
          * been set to true by the root thread
          */
		  monitor ="Error";
      }
   }

   const ImageVariant* TargetImage () const
   {
      return target;
   }

   const ImageVariant* DrizzleImage () const
   {
      return drzImage;
   }

   String TargetPath () const
   {
      return targetPath;
   }

   String DrizzlePath () const
   {
      return drzPath;
   }

   void TargetPath(const String p)
   {
      targetPath = p;
   }

   void DrizzlePath(const String p)
   {
      drzPath = p;
   }

   FileData* GetFileData () const
   {
      return fileData;
   }
      
   FileData* GetDrzData () const
   {
      return drzData;
   }
   
   DPoint Delta () const
   {
      return delta;
   }
      
   bool isDrizzle () const
   {
      return drizzle;
   }
    
   Matrix DrzMatrix() const
   {
      return drzMatrix;
   }
      
   LinearFitEngine::linear_fit_set GetLinearFitSet() const
   {
      return LFSet;
   }

   const ImageVariant StarAligned() const
   {
      return saImg;
   }
     
   const ImageVariant CometAligned() const
   {
      return caImg;
   }

private:
	const CometAlignmentInstance* i; //instance
	ImageVariant* target; // source starAligned image, or in drizzle mode the original drizzle integrable image
	FileData* fileData; // Target image parameters and embedded data. It belongs to this thread.
	ImageVariant* drzImage; // original drizzle integrable image
	FileData* drzData; // original drizzle integrable image parameters and embedded data.
	String targetPath; // path to source starAligned image
	String drzPath;	// path to original drizzle integrable image
	DPoint delta; // Comet movement Delta x, y around drzMatrix 
	bool drizzle; // true == drizzle mode
	Matrix drzMatrix; //drizzle AlignmentMatrix
	const ImageVariant* operand; //Image for subtraction from target
	LinearFitEngine::linear_fit_set LFSet;
	ImageVariant saImg; //pureStarAligned
	ImageVariant caImg; //pureCometAligned
	
   
   template <class P>
   void Normalize (GenericImage<P>& img) //subtract Median from Comet image
   {
	   for (int c = 0; c < img.NumberOfNominalChannels (); ++c)
	   {
		   typename P::sample m = img.Median (0, c, c);
		   typename P::sample* v = img.PixelData (c);
		   typename P::sample* vN = v + img.NumberOfPixels ();
		   for (; v < vN; ++v)
			   if (*v > 0) //ignore black pixels
				   *v -= m;
	   }
   }
   
   void Normalize (ImageVariant& cimg)
   {
	   monitor = "Normalization";
	   if (!i->p_normalize || cimg.IsComplexSample() )
		   return;
	   #if debug
	   Console().Write("Normalize ");
	   #endif
	   if (cimg.IsFloatSample ())
		   switch (cimg.BitsPerSample ())
	   {
		   case 32: Normalize (static_cast<Image&> (*cimg)); break;
		   case 64: Normalize (static_cast<DImage&> (*cimg)); break;
	   }
	   else 
		   switch (cimg.BitsPerSample ())
	   {
		   case 8: Normalize (static_cast<UInt8Image&> (*cimg)); break;
		   case 16: Normalize (static_cast<UInt16Image&> (*cimg)); break;
		   case 32: Normalize (static_cast<UInt32Image&> (*cimg)); break;
	   }
   }

   template <class P>
   void HomographyApplyTo (GenericImage<P>& input, const Matrix M)
	{
		Homography H(M);
		int wi = input.Width();
		int hi = input.Height();
		int n = input.NumberOfNominalChannels();
		int n1 = input.NumberOfChannels();		
			
		GenericImage<P> output;			
		output.AllocateData(wi, hi, n1, input.ColorSpace());     
			
		IndirectArray<PixelInterpolation::Interpolator<P> > interpolators( n1 );
		for ( int c = 0; c < n1; ++c )
		{
			int c0 = (c < n) ? Min( c, n-1 ) : Min( c-n, n1-n-1 ) + n;
			interpolators[c] = pixelInterpolation->NewInterpolator( (P*)0, input.PixelData( c0 ), wi, hi );
		}
		
		for ( int y = 0; y < hi; ++y)
		{
			for ( int x = 0; x < wi; ++x )
			{
				DPoint p = H(x,y); //caclulate source point via Homography
				if ( p.x >= 0 && p.x < wi && p.y >= 0 && p.y < hi ) // ignore out of bounds points
				{
					for ( int c = 0; c < n1; ++c )
					{
						output.Pixel(x,y,c) = (*interpolators[c])( p );
					}
				}
			}
			if ( TryIsAborted() )
				return;
			monitor2 = y;
		}
		interpolators.Destroy();
		input.Assign(output);
	}
 
   void HomographyApplyTo(ImageVariant& image, const Matrix M )
	{
		if (!image.IsComplexSample ())
		{
      if (image.IsFloatSample ())
         switch (image.BitsPerSample ())
         {
         case 32: HomographyApplyTo (static_cast<Image&> (*image),M);
            break;
         case 64: HomographyApplyTo (static_cast<DImage&> (*image),M);
            break;
         }
      else
         switch (image.BitsPerSample ())
         {
         case 8: HomographyApplyTo (static_cast<UInt8Image&> (*image),M);
            break;
         case 16: HomographyApplyTo (static_cast<UInt16Image&> (*image),M);
            break;
         case 32: HomographyApplyTo (static_cast<UInt32Image&> (*image),M);
            break;
         }		
		}
	}
};

// ----------------------------------------------------------------------------

template <class P>
static void LoadImageFile (GenericImage<P>& image, FileFormatInstance& file)
{
   if (!file.ReadImage (image))
      throw CatchedException ();
}
static void LoadImageFile (ImageVariant& v, FileFormatInstance& file, ImageOptions options)
{
   if (!file.SelectImage (0))
      throw CatchedException ();
	v.CreateSharedImage (options.ieeefpSampleFormat, false, options.bitsPerSample);

   if (v.IsFloatSample ()) switch (v.BitsPerSample ())
      {
      case 32: LoadImageFile (*static_cast<Image*> (v.AnyImage ()), file);
         break;
      case 64: LoadImageFile (*static_cast<DImage*> (v.AnyImage ()), file);
         break;
      }
   else switch (v.BitsPerSample ())
      {
      case 8: LoadImageFile (*static_cast<UInt8Image*> (v.AnyImage ()), file);
         break;
      case 16: LoadImageFile (*static_cast<UInt16Image*> (v.AnyImage ()), file);
         break;
      case 32: LoadImageFile (*static_cast<UInt32Image*> (v.AnyImage ()), file);
         break;
      }
	  
}
FileData* CometAlignmentInstance::CAReadImage(ImageVariant* img, const String& path)
{
	FileFormat format (File::ExtractExtension (path), true, false);
	FileFormatInstance file (format);
	ImageDescriptionArray images;
	if ( !file.Open( images, path, p_inputHints ) ) throw CatchedException ();
	if (images.IsEmpty ()) throw Error (path + ": Empty image file.");
	if (images.Length () > 1) throw Error ("Multiple image files is not supported.");
	LoadImageFile (*img, file, images[0].options);
	//ImageVariant2ImageWindow(img); //show loaded image
	ProcessInterface::ProcessEvents ();
	if (m_geometry.IsRect ())
	{
		if (img->Bounds () != m_geometry) throw Error ("The Image geometry is not equal.");
	}
	else
		m_geometry = img->Bounds ();
	
	FileData* inputData = new FileData(file, images[0].options);

	Console().WriteLn ("Close " + path);
	file.Close ();
	return inputData;
}
static DrizzleSADataDecoder ReadDrizzleFile( const String drzFile)
{
	File file;
	file.OpenForReading( drzFile );
	fsize_type fileSize = file.Size();
	IsoString text;
	if ( fileSize > 0 )
	{
		text.Reserve( fileSize );
		file.Read( reinterpret_cast<void*>( text.Begin() ), fileSize );
		text[fileSize] = '\0';
	}
	file.Close();
	DrizzleSAFilter filter;
	text = filter.Filter( text );
	if ( text.IsEmpty() )
		throw Error( "The drizzle file has no image alignment data: " + drzFile );
	if ( filter.HasSplines() )
		throw Error( "Surface splines are not supported for drizzle by this version of CometAlignment: " + drzFile );
	if ( !filter.HasMatrix() )
		throw Error( "The drizzle file does not define an alignment matrix: " + drzFile );
	DrizzleSADataDecoder decoder;
	decoder.Decode( text );
	return decoder;
}

inline thread_list CometAlignmentInstance::LoadTargetFrame (const size_t fileIndex)
{
	/*
	read Target image
	if .drz
	{
		extract from .drz drzMatrix
		extract from .drz drzSourcePath
		if (operand) read drzImage(drzSourcePath)
	}

	*/
	Console console;
	String targetPath;
	ImageItem& item = p_targetFrames[fileIndex];
	ImageItem& r = p_targetFrames[p_reference];
	DPoint delta (item.x - r.x, item.y - r.y);
	
	ImageVariant *drzImage;
	FileData *targetData, *drzData;
	thread_list threads;
	ImageVariant* targetImage = 0;
	drzImage = 0;
	targetData = drzData = 0;

	try
	{
		//read target
		targetImage = new ImageVariant();
		targetPath = item.path;
		targetData = CAReadImage(targetImage, targetPath);		

		//read drizzle
		String drzSourcePath;
		Matrix drzMatrix;
		String drzFile = item.drzPath;
		bool drizzle(!drzFile.IsEmpty());
		
		if(drizzle)
		{ 
			DrizzleSADataDecoder decoder;
			decoder = ReadDrizzleFile(drzFile);
			drzSourcePath = decoder.FilePath();
			drzMatrix = decoder.AlignmentMatrix();

			if(m_OperandImage)
			{
				drzImage = new ImageVariant();
				drzData = CAReadImage(drzImage, drzSourcePath);
			}
		}
		
		threads.Add (new CAThread (targetImage, targetData, drzImage, drzData, targetPath, drzSourcePath, delta, drzMatrix, this));

		return threads;
   }
   catch (...)
   {
      if (targetImage != 0) delete targetImage;
	  if (targetData != 0) delete targetData;
	  if (drzImage != 0) delete drzImage;
	  if (drzData != 0) delete drzData;
      throw;
   }
}

// ----------------------------------------------------------------------------

inline void SaveDrizzleFile( const String& i, const String& o, const Matrix& H, const int w, const int h )
{
	#if debug
	Console().WriteLn ("drz Source:"+i);
	Console().WriteLn ("drz Target:"+o);
	#endif
	if ( i == o )
      throw Error( "SaveDrizzleFile(): Internal error: Source and destination .drz files must be different." );
	String outputDrizleFile(File::ChangeExtension (o,".drz"));
 
   Console().WriteLn ("Write drizzle file: " +outputDrizleFile);
   File file;
   file.CreateForWriting( outputDrizleFile );
   file.OutText( "P{" );
   file.OutText( IsoString( i.ToUTF8() ) ); // drizzle integrable source image
   file.OutText( "}" );
   file.OutText( "T{" );
   file.OutText( IsoString( o.ToUTF8() ) );  // registration target image
   file.OutText( "}" );
   file.OutText( IsoString().Format( "D{%d,%d}", w, h ) ); // width, height
	#if debug
   for(int i=0;i<3;i++)
	   for(int j=0;j<3;j++)
		   Console().WriteLn (String().Format("H[%d][%d]:%.16g",i,j,H[i][j]));
	#endif
   file.OutText( IsoString().Format( "H{%.16g,%.16g,%.16g,%.16g,%.16g,%.16g,%.16g,%.16g,%.16g}",
                    H[0][0], H[0][1], H[0][2],
                    H[1][0], H[1][1], H[1][2],
                    H[2][0], H[2][1], H[2][2] ) );
   file.Close();
}

template <class P>
static void SaveImageFile (const GenericImage<P>& image, FileFormatInstance& file)
{
   if (!file.WriteImage (image))
      throw CatchedException ();
}

static void SaveImageFile (const ImageVariant& image, FileFormatInstance& file)
{
   if (image.IsFloatSample ()) switch (image.BitsPerSample ())
      {
      case 32: SaveImageFile (*static_cast<const Image*> (image.AnyImage ()), file);
         break;
      case 64: SaveImageFile (*static_cast<const DImage*> (image.AnyImage ()), file);
         break;
      }
   else switch (image.BitsPerSample ())
      {
      case 8: SaveImageFile (*static_cast<const UInt8Image*> (image.AnyImage ()), file);
         break;
      case 16: SaveImageFile (*static_cast<const UInt16Image*> (image.AnyImage ()), file);
         break;
      case 32: SaveImageFile (*static_cast<const UInt32Image*> (image.AnyImage ()), file);
         break;
      }
}

inline String UniqueFilePath (const String& filePath)
{
   for (unsigned u = 1;; ++u)
   {
      String tryFilePath = File::AppendToName (filePath, '_' + String (u));
      if (!File::Exists (tryFilePath))
         return tryFilePath;
   }
}

inline String CometAlignmentInstance::OutputImgPath (const String& imgPath, const String& postfix)
{
   String dir = p_outputDir;
   dir.Trim ();
   if (dir.IsEmpty ())
      dir = File::ExtractDrive (imgPath) + File::ExtractDirectory (imgPath);
   if (dir.IsEmpty ())
      throw Error (dir + ": Unable to determine an output p_outputDir.");
   if (!dir.EndsWith ('/'))
      dir.Append ('/');

   String fileName = File::ExtractName (imgPath);
   fileName.Trim ();
   if (!p_prefix.IsEmpty ()) fileName.Prepend (p_prefix);
   if (!p_postfix.IsEmpty ()) fileName.Append (postfix);
   if (fileName.IsEmpty ()) throw Error (imgPath + ": Unable to determine an output file name.");

   String outputFilePath = dir + fileName + p_outputExtension;
   //Console ().WriteLn ("<end><cbr><br>Writing output file: " + outputFilePath);

   if (File::Exists (outputFilePath))
      if (p_overwrite)
         Console ().WarningLn ("** Warning: Overwriting already existing file.");
      else
      {
         outputFilePath = UniqueFilePath (outputFilePath);
         Console ().NoteLn ("* File already exists, writing to: " + outputFilePath);
      }

   return outputFilePath;
}

void LFReport(const LinearFitEngine::linear_fit_set L) 
{
	Console().WriteLn( "<end><cbr>Linear fit functions:" );
	for ( int c = 0; c <  L.Length() ; ++c )
	{
		Console().WriteLn( String().Format( "y<sub>%d</sub> = %+.6f %c %.6f&middot;x<sub>%d</sub>", c, L[c].a, (L[c].b < 0) ? '-' : '+', Abs( L[c].b ), c ) );
		Console().WriteLn( String().Format( "&sigma;<sub>%d</sub> = %+.6f", c, L[c].adev ) );
	}
}
void CometAlignmentInstance::Save(const ImageVariant* img, CAThread* t, const int8 mode)
{
	//mode ==
	//0 == Save target result image
	//1 == Save new NonAligned image

	Console console;
	String inputImgPath;
	String postfix;
	FileData* data;
	if(mode == 0 )
	{
		data = t->GetFileData();
		postfix = p_postfix;
		inputImgPath = t->TargetPath();				//target source image path	
		//data = t->GetFileData();
	}
	else
	{
		data = t->GetDrzData();
		postfix.Clear();
		inputImgPath = t->DrizzlePath();			//drizzle source image path	
		//data = t->GetDrzData();
	}

	bool drizzle(t->isDrizzle());					//true == drizle used
	bool operand(!p_subtractFile.IsEmpty());		//true == operand used
	DPoint delta(t->Delta());						//comet movement delta

	LinearFitEngine::linear_fit_set L;				//LinearFit result
	if ( operand && p_enableLinearFit )
	{
      L = t->GetLinearFitSet();
	  LFReport(L);
	}	


   String outputImgPath = OutputImgPath (inputImgPath, postfix); //convert inputImgPath to output image
   console.WriteLn ("Create " + outputImgPath);

   FileFormat outputFormat (p_outputExtension, false, true);
   FileFormatInstance outputFile (outputFormat);
   if ( !outputFile.Create( outputImgPath, p_outputHints ) ) throw CatchedException ();

//   const FileData& data = t->GetFileData ();   

   ImageOptions options = data->options; // get resolution, etc.
   outputFile.SetOptions (options);

   if (data->fsData != 0)
      if (outputFormat.ValidateFormatSpecificData (data->fsData)) outputFile.SetFormatSpecificData (data->fsData);

   if (outputFormat.CanStoreKeywords ())
   {
      FITSKeywordArray keywords = data->keywords;
      keywords.Add (FITSHeaderKeyword ("COMMENT", IsoString (), "CometAlignment with " + PixInsightVersion::AsString ()));
      keywords.Add (FITSHeaderKeyword ("COMMENT", IsoString (), "CometAlignment with " + CometAlignmentModule::ReadableVersion ()));
      if (!p_subtractFile.IsEmpty())
      {
         keywords.Add (FITSHeaderKeyword ("HISTORY", IsoString (), "CometAlignment.Subtract: " + IsoString( p_subtractFile ) ) );
         keywords.Add (FITSHeaderKeyword ("HISTORY", IsoString (), "CometAlignment.Mode: " + IsoString( p_subtractMode ? "true" : "false" ) ) );
         keywords.Add (FITSHeaderKeyword ("HISTORY", IsoString (), "CometAlignment.LinearFit: " + IsoString( p_enableLinearFit ? "true" : "false" ) ) );
         keywords.Add (FITSHeaderKeyword ("HISTORY", IsoString (), "CometAlignment.RejectLow: " + IsoString( p_rejectLow ) ) );
         keywords.Add (FITSHeaderKeyword ("HISTORY", IsoString (), "CometAlignment.RejectHigh: " + IsoString( p_rejectHigh ) ) );
         if (p_enableLinearFit)
         {
            keywords.Add (FITSHeaderKeyword ("HISTORY", IsoString (), "CometAlignment.Linear fit functions:" ) );
            for ( int c = 0; c < L.Length(); ++c )
            {
               keywords.Add (FITSHeaderKeyword ("HISTORY", IsoString (), IsoString().Format( "y%d = %+.6f %c %.6f * x%d", c, L[c].a, (L[c].b < 0) ? '-' : '+', Abs( L[c].b ), c ) ) );
               keywords.Add (FITSHeaderKeyword ("HISTORY", IsoString (), IsoString().Format( "sigma%d = %+.6f", c, L[c].adev ) ) );
            }
         }
         keywords.Add (FITSHeaderKeyword ("HISTORY", IsoString (), "CometAlignment.Normalize: " + IsoString( p_normalize ? "true" : "false") ) );
      }

	  keywords.Add (FITSHeaderKeyword ("HISTORY", IsoString (), "CometAlignment.X: " + IsoString(delta.x)));
      keywords.Add (FITSHeaderKeyword ("HISTORY", IsoString (), "CometAlignment.Y:" +IsoString(delta.y)));

      outputFile.Embed (keywords);
   }
   else if (!data->keywords.IsEmpty ())
      console.WarningLn ("** Warning: The output format cannot store FITS keywords - original keywords not embedded.");

   if (data->profile.IsProfile ())
      if (outputFormat.CanStoreICCProfiles ()) outputFile.Embed (data->profile);
      else console.WarningLn ("** Warning: The output format cannot store ICC profiles - original profile not embedded.");

   if (!data->metadata.IsEmpty ())
      if (outputFormat.CanStoreMetadata ()) outputFile.Embed (data->metadata.Begin (), data->metadata.Length ());
      else console.WarningLn ("** Warning: The output format cannot store metadata - original metadata not embedded.");

   SaveImageFile (*img, outputFile);
	#if debug
	console.WriteLn ("Close file.");
	#endif
   outputFile.Close ();
   
	if (drizzle)
	{
		if ( mode == 0)
		{  
			//update patch to real stored image name
			#if debug
			console.WriteLn ("Update TargetPath to: " +outputImgPath);
			#endif
			t->TargetPath(outputImgPath); 
		}
		else if( operand ) //operand && mode == 1
		{	//we create new NonRegistered image -> store path to CAThread for re use in .drz file
			#if debug
			console.WriteLn ("Update DrizzlePath to: " +outputImgPath);
			#endif
			t->DrizzlePath(outputImgPath); 
		}
	}
}
void CometAlignmentInstance::SaveImage( CAThread* t)
{	
	Console().WriteLn();
	Console().WriteLn ("Save Target");
	Save(t->TargetImage(), t, 0);			//Save target result image
	
	if (!t->DrizzlePath().IsEmpty())
	{
		if(t->DrizzleImage())					//Save new NonAligned image	
		{
			Console().WriteLn ("Save new NonAligned");
			Save(t->DrizzleImage(), t, 1);	
		}
		Matrix M = t->DrzMatrix(); // starAlignment matrix
		
		if (p_subtractFile.IsEmpty() || !p_subtractMode) //no operand or operand is StarIntegration
		{
			M = M * DeltaToMatrix(t->Delta()); // add comet movement delta
			M /= M[2][2];
		}
	
		Console().WriteLn ("Save .drz file");
		SaveDrizzleFile( t->DrizzlePath(), t->TargetPath(), M, t->TargetImage()->Width(), t->TargetImage()->Height() );
	}
	/*
	if(t->StarAligned())					//Save PureStarAligned
	{
		ImageVariant* img = new ImageVariant(t->StarAligned());
		Save(img, t, 2);
		delete img, img = 0;
	}
	if(t->CometAligned())					//Save PureCometAligned
	{
		ImageVariant* img = new ImageVariant(t->StarAligned());
		Save(img, t, 3);
		delete img, img = 0;
	}
		*/
}

// ----------------------------------------------------------------------------

inline void CometAlignmentInstance::InitPixelInterpolation ()
{
   Console ().WriteLn ("PixelInterpolation: " + ThePixelInterpolationParameter->ElementId (p_pixelInterpolation));
   if (p_pixelInterpolation == CAPixelInterpolation::BicubicSpline) Console ().WriteLn ("ClampingThreshold : " + String (p_linearClampingThreshold));

   switch (p_pixelInterpolation)
   {
   case CAPixelInterpolation::NearestNeighbor:
      pixelInterpolation = new NearestNeighborPixelInterpolation;
      break;
   case CAPixelInterpolation::Bilinear:
      pixelInterpolation = new BilinearPixelInterpolation;
      break;
   default:
   case CAPixelInterpolation::BicubicSpline:
      pixelInterpolation = new BicubicSplinePixelInterpolation (p_linearClampingThreshold);
      break;
   case CAPixelInterpolation::BicubicBSpline:
      pixelInterpolation = new BicubicBSplinePixelInterpolation;
      break;
   case CAPixelInterpolation::Lanczos3:
   case CAPixelInterpolation::Lanczos4:
   case CAPixelInterpolation::Lanczos5:

   {
      // ### FIXME: Use LUT Lanczos interpolations for 8-bit and 16-bit images

      // Disable clamping when clampingThreshold == 1
      float clamp = (p_linearClampingThreshold < 1) ? p_linearClampingThreshold : -1;
      switch (p_pixelInterpolation)
      {
      default:
      case CAPixelInterpolation::Lanczos3:
         pixelInterpolation = new LanczosPixelInterpolation (3, clamp);
         break;
      case CAPixelInterpolation::Lanczos4:
         pixelInterpolation = new LanczosPixelInterpolation (4, clamp);
         break;
      case CAPixelInterpolation::Lanczos5:
         pixelInterpolation = new LanczosPixelInterpolation (5, clamp);
         break;
      }
   }
      break;
   case CAPixelInterpolation::MitchellNetravaliFilter:
      pixelInterpolation = new BicubicFilterPixelInterpolation (2, 2, MitchellNetravaliCubicFilter ());
      break;
   case CAPixelInterpolation::CatmullRomSplineFilter:
      pixelInterpolation = new BicubicFilterPixelInterpolation (2, 2, CatmullRomSplineFilter ());
      break;
   case CAPixelInterpolation::CubicBSplineFilter:
      pixelInterpolation = new BicubicFilterPixelInterpolation (2, 2, CubicBSplineFilter ());
      break;
   }
}

inline ImageVariant* CometAlignmentInstance::LoadOperandImage (const String& filePath)
{
   Console console;
   if (filePath.IsEmpty ())
      return 0;
   console.WriteLn ("Open " + filePath);
   FileFormat format (File::ExtractExtension (filePath), true, false);
   FileFormatInstance file (format);
   ImageDescriptionArray images;
   if ( !file.Open( images, filePath, p_inputHints ) ) throw CatchedException ();
   if (images.IsEmpty ()) throw Error (filePath + ": Empty image file.");
   ImageVariant* img = new ImageVariant ();
	LoadImageFile (*img, file, images[0].options);
   ProcessInterface::ProcessEvents ();
   console.WriteLn ("Close " + filePath);
   file.Close ();
   m_geometry = img->Bounds ();
   return img;
}

// ----------------------------------------------------------------------------

bool CometAlignmentInstance::ExecuteGlobal ()
{
   Console console;
   console.Show ();
   
   String why;
   if (!CanExecuteGlobal (why)) throw Error (why);

   m_geometry = 0;
   m_OperandImage = 0;

   TreeBox monitor = TheCometAlignmentInterface->GUI->Monitor_TreeBox; 

   try //try 1
   {
      Exception::EnableConsoleOutput ();
      Exception::DisableGUIOutput ();
      console.EnableAbort ();

      m_OperandImage = LoadOperandImage (p_subtractFile);
      if (m_OperandImage)
      {
         if (p_subtractMode)
            console.WriteLn ("Mode: Align Operand image and subtract from Targets.");
         else
            console.WriteLn ("Mode: Subtract operand from Targets and align.");

         console.WriteLn ("LinearFit " + String (p_enableLinearFit ? "Enabled" : "Disabled")
                          + String ().Format (", rejection Low:%f, High:%f", p_rejectLow, p_rejectHigh));
         console.WriteLn ("Normalization " + String (p_normalize ? "Enabled" : "Disabled"));
      }
      else
         console.WriteLn ("Mode: Only align Target images.");

      InitPixelInterpolation ();

      size_t succeeded = 0;
      size_t skipped = 0;
      const size_t total = p_targetFrames.Length ();
      console.WriteLn (String ().Format ("<br>Processing %u target frames:", total));

      Array<size_t> t;
      for (size_t i = 0; i < total; t.Add (i++)); // Array with file indexes

      const int totalCPU = Thread::NumberOfThreads (1024, 1);
      Console ().Write (String ().Format ("Detected %u CPU. ", totalCPU));

      const size_t n = Min (size_t (totalCPU), total);
      thread_list runningThreads (n); // n = how many threads will run simultaneously
      console.WriteLn (String ().Format ("Using %u worker threads", runningThreads.Length ()));

      thread_list waitingThreads; //container for hold images from next image. One or more if file is multi image	
	  
	  // Create Monitor to show processing status -------------------------------------------
	  // Hide main GUI
	  TheCometAlignmentInterface->GUI->Interpolation_Control.Hide();	  
	  TheCometAlignmentInterface->GUI->Interpolation_SectionBar.Hide();
	  TheCometAlignmentInterface->GUI->FormatHints_Control.Hide();
	  TheCometAlignmentInterface->GUI->FormatHints_SectionBar.Hide();
	  TheCometAlignmentInterface->GUI->Output_Control.Hide(); 
	  TheCometAlignmentInterface->GUI->Output_SectionBar.Hide();
	  TheCometAlignmentInterface->GUI->Parameter_Control.Hide();  
	  TheCometAlignmentInterface->GUI->Parameter_SectionBar.Hide(); 
	  TheCometAlignmentInterface->GUI->Subtract_Control.Hide();
	  TheCometAlignmentInterface->GUI->Subtract_SectionBar.Hide();
	  TheCometAlignmentInterface->GUI->TargetImages_Control.Hide();
	  TheCometAlignmentInterface->GUI->TargetImages_SectionBar.Hide();
//	  const bool fullPath(TheCometAlignmentInterface->GUI->FullPaths_CheckBox.IsChecked());
   
	  monitor.SetFixedHeight((runningThreads.Length()+1)*(1.5*monitor.Font().Height())); //set monitor height according qty of runningThreads Length + 1 for header

	  for(int cpu=0; cpu < int(runningThreads.Length()); cpu++)
		  (new TreeBox::Node(monitor))->SetText (0, String (cpu));	 

	  monitor.AdjustColumnWidthToContents(0);

	  
	  monitor.SetColumnWidth(1,TheCometAlignmentInterface->GUI->TargetImages_TreeBox.ColumnWidth(2)); //== fileName width 
	  monitor.SetColumnWidth(2,monitor.Font().Width(String("Align PureComet")+"MM"));
	  monitor.SetColumnWidth(3,TheCometAlignmentInterface->GUI->TargetImages_TreeBox.ColumnWidth(5)); //== Y width

	  // Correcting width of Monitor_TreeBox -------------------------------------------
	  monitor.Show();	  
	  const int lastColumn = monitor.NumberOfColumns() - 1;
	  monitor.ShowColumn (lastColumn); // temporarry show last column, which uset only for GUI width expansion
	  monitor.SetColumnWidth (lastColumn, 0); // set width of last column to zero
	  int width = 0;
	  for (int i = 0; i < lastColumn; i++)
		  width += monitor.ColumnWidth (i); // calculate total width of columns

	  monitor.SetFixedWidth (width);
	  monitor.HideColumn (lastColumn); // hide last column to hide horisontal scroling
	  
	  TheCometAlignmentInterface->Restyle ();
	  TheCometAlignmentInterface->AdjustToContents();
      try //try 2
      {
         int runing = 0; // runing == Qty images processing now == Qty CPU isActiv now.
         do
         {
            Module->ProcessEvents (); // Keep the GUI responsive
            if (console.AbortRequested ()) throw ProcessAborted ();

            // ------------------------------------------------------------
            // Open File
            if (!t.IsEmpty () && waitingThreads.IsEmpty ())
            {
               size_t fileIndex = *t; // take first index from begining of the list
               t.Remove (t.Begin ()); // remove the index from the list

               console.WriteLn (String ().Format ("<br>File %u of %u", total - t.Length (), total));
               if (p_targetFrames[fileIndex].enabled)
                  waitingThreads = LoadTargetFrame (fileIndex); // put all sub-images from file to waitingThreads
               else
               {
                  ++skipped;
                  console.NoteLn ("* Skipping disabled target");
               }
            }

            // ------------------------------------------------------------
            // Find idle or free CPU
            thread_list::iterator i = 0;
			int cpu =0;
            for (thread_list::iterator j = runningThreads.Begin (); j != runningThreads.End (); ++j, ++cpu) //Cycle in CPU units
            {
               if (*j == 0) // the CPU is free and empty.
               {
                  if (!waitingThreads.IsEmpty ()) // there are not processed images
                  {
                     i = j;
                     break; // i pointed to CPU which is free now.
                  }
               }// *j != 0 the CPU is non free and maybe idle or active
               else if (!(*j)->IsActive ()) // the CPU is idle = the CPU has finished processing
               {
                  i = j;
                  break; // i pointed to CPU thread which ready to save.
               }
			   else //the CPU IsActive
			   {
				   if(!(*j)->monitor.IsEmpty() || (*j)->monitor2 != 0 ) //If need update Status or Row into Monitor window
				   {
					   TreeBox::Node* node = monitor[cpu]; //link from CPU# to Monitor node  
					   if(!(*j)->monitor.IsEmpty() )
					   {
						   node->SetText (2, (*j)->monitor ); //Show processing Status in Monitor
						   (*j)->monitor.Clear();
						   node->SetText (3, "");
					   }
					   if((*j)->monitor2 != 0 )
					   {
						   node->SetText (3, String((*j)->monitor2)); //Show processing Row in Monitor
						   (*j)->monitor2 = 0;
					   }
				   }
			   }
            }

            if (i == 0) // all CPU IsActive or no new images
            {
               pcl::Sleep (0.1);
               continue;
            }

            // ------------------------------------------------------------
            // Write File
            if (*i != 0) //the CPU is idle
            {
               runing--;
               try
               {				
                  console.WriteLn (String ().Format ("<br>CPU#%u has finished processing.", cpu ));
				  (*i)->FlushConsoleOutputText();
				  TreeBox::Node* node = monitor[cpu];
				  node->SetText (2, "Save"); //Status
				  node->SetText (3, ""); //Y

                  SaveImage (*i);
                  
				  runningThreads.Delete (i); //prepare thread for next image. now (*i == 0) the CPU is free
				  
				  //node->SetText (0, ""); //CPU#
				  node->SetText (1, ""); //File	
				  node->SetText (2, ""); //Status
				  
               }
               catch (...)
               {
                  runningThreads.Delete (i);
                  throw;
               }
               ++succeeded;
            } //now (*i == 0) the CPU is free

			// Keep the GUI responsive
			Module->ProcessEvents();
			if ( console.AbortRequested() )
				throw ProcessAborted();    

            // ------------------------------------------------------------
            // Put image to empty runningThreads slot and Run()

            if (!waitingThreads.IsEmpty ())
            {
               *i = *waitingThreads; //put one sub-image to runningThreads. so, now (*i != 0)
               waitingThreads.Remove (waitingThreads.Begin ()); //remove one sub-image from waitingThreads
               console.WriteLn (String ().Format ("<br>CPU#%u processing file ", cpu ) + (*i)->TargetPath());
 
			   (*i)->Start (ThreadPriority::DefaultMax, i - runningThreads.Begin ());
               runing++;
			   
			   TreeBox::Node* node = monitor[cpu];
			   node->SetText (1, File::ExtractName( (*i)->TargetPath() ) ); //file
			   node->SetText (2, "Run"); //status			   
            }
         }
         while (runing > 0 || !t.IsEmpty ());
      }// try 2
      catch (...)
      {
         try
         {
            throw;
         }
         ERROR_HANDLER;
		 
		 console.NoteLn( "<end><cbr><br>* Waiting for running tasks to terminate ..." );
		 for ( thread_list::iterator i = runningThreads.Begin(); i != runningThreads.End(); ++i )
			 if ( *i != 0 ) (*i)->Abort();
		 for ( thread_list::iterator i = runningThreads.Begin(); i != runningThreads.End(); ++i )
			 if ( *i != 0 ) (*i)->Wait();
		 runningThreads.Destroy();
		 waitingThreads.Destroy();
		 throw;
      }

      console.NoteLn (String ().Format ("<br>===== CometAlignment: %u succeeded, %u skipped, %u canceled =====",
                                        succeeded, skipped, total - succeeded));

      Exception::DisableConsoleOutput ();
      Exception::EnableGUIOutput ();

      if (m_OperandImage != 0)
         delete m_OperandImage, m_OperandImage = 0;

	  monitor.Clear();
	  monitor.Hide();
	  TheCometAlignmentInterface->GUI->Interpolation_SectionBar.Show();
	  TheCometAlignmentInterface->GUI->FormatHints_SectionBar.Show();
	  TheCometAlignmentInterface->GUI->Output_SectionBar.Show();
	  TheCometAlignmentInterface->GUI->Parameter_SectionBar.Show(); 
	  TheCometAlignmentInterface->GUI->Subtract_SectionBar.Show();
	  TheCometAlignmentInterface->GUI->TargetImages_SectionBar.Show();
	  TheCometAlignmentInterface->GUI->TargetImages_Control.Show();
	  TheCometAlignmentInterface->AdjustToContents(); 
   
      return true;
   }
   catch (...)
   {
      Exception::DisableConsoleOutput ();
      Exception::EnableGUIOutput ();
      if (m_OperandImage != 0) delete m_OperandImage, m_OperandImage = 0;
  
	  monitor.Clear();
	  monitor.Hide();
	  TheCometAlignmentInterface->GUI->Interpolation_SectionBar.Show();
	  TheCometAlignmentInterface->GUI->FormatHints_SectionBar.Show();
	  TheCometAlignmentInterface->GUI->Output_SectionBar.Show();
	  TheCometAlignmentInterface->GUI->Parameter_SectionBar.Show(); 
	  TheCometAlignmentInterface->GUI->Subtract_SectionBar.Show();
	  TheCometAlignmentInterface->GUI->TargetImages_SectionBar.Show();
	  TheCometAlignmentInterface->GUI->TargetImages_Control.Show();
	  TheCometAlignmentInterface->AdjustToContents(); 

      console.NoteLn ("<end><cbr><br>* CometAlignment terminated.");
      throw;
   }
}

// ----------------------------------------------------------------------------

void* CometAlignmentInstance::LockParameter (const MetaParameter* p, size_type tableRow)
{
   if (p == TheTargetFrameEnabled) return &p_targetFrames[tableRow].enabled;
   if (p == TheTargetFramePath) return p_targetFrames[tableRow].path.c_str ();
   if (p == TheTargetFrameDate) return p_targetFrames[tableRow].date.c_str ();
   if (p == TheTargetFrameJDate)return &p_targetFrames[tableRow].Jdate;
   if (p == TheTargetFrameX) return &p_targetFrames[tableRow].x;
   if (p == TheTargetFrameY) return &p_targetFrames[tableRow].y;
   if (p == TheDrizzlePath ) return p_targetFrames[tableRow].drzPath.c_str();

   if (p == TheCAInputHintsParameter ) return p_inputHints.c_str();
   if (p == TheCAOutputHintsParameter ) return p_outputHints.c_str();
   if (p == TheOutputDir) return p_outputDir.c_str ();
   if (p == TheCAOutputExtensionParameter ) return p_outputExtension.c_str();
   if (p == ThePrefix) return p_prefix.c_str ();
   if (p == ThePostfix) return p_postfix.c_str ();
   if (p == TheOverwrite) return &p_overwrite;

   if (p == TheReference) return &p_reference;

   if (p == TheSubtractFile) return p_subtractFile.c_str ();
   if (p == TheSubtractMode) return &p_subtractMode;
   if (p == TheOperandIsDI) return &p_OperandIsDI;
   if (p == TheNormalize) return &p_normalize;
   if (p == TheEnableLinearFit) return &p_enableLinearFit;
   if (p == TheRejectLow) return &p_rejectLow;
   if (p == TheRejectHigh) return &p_rejectHigh;
   if (p == TheDrzSaveSA) return &p_drzSaveSA;
   if (p == TheDrzSaveCA) return &p_drzSaveCA;

   if (p == TheLinearClampingThresholdParameter) return &p_linearClampingThreshold;
   if (p == ThePixelInterpolationParameter) return &p_pixelInterpolation;
   return 0;
}

bool CometAlignmentInstance::AllocateParameter (size_type sizeOrLength, const MetaParameter* p, size_type tableRow)

{
   if (p == TheTargetFrames)
   {
      p_targetFrames.Clear ();
      if (sizeOrLength > 0) p_targetFrames.Add (ImageItem (), sizeOrLength);
   }
   else if (p == TheTargetFramePath)
   {
      p_targetFrames[tableRow].path.Clear ();
      if (sizeOrLength > 0) p_targetFrames[tableRow].path.Reserve (sizeOrLength);
   }
   else if (p == TheTargetFrameDate)
   {
      p_targetFrames[tableRow].date.Clear ();
      if (sizeOrLength > 0) p_targetFrames[tableRow].date.Reserve (sizeOrLength);
   }
   else if ( p == TheDrizzlePath )
   {
      p_targetFrames[tableRow].drzPath.Clear();
      if ( sizeOrLength > 0 )
         p_targetFrames[tableRow].drzPath.Reserve( sizeOrLength );
   }
   else if ( p == TheCAInputHintsParameter )
   {
      p_inputHints.Clear();
      if ( sizeOrLength > 0 )
         p_inputHints.Reserve( sizeOrLength );
   }
   else if ( p == TheCAOutputHintsParameter )
   {
      p_outputHints.Clear();
      if ( sizeOrLength > 0 )
         p_outputHints.Reserve( sizeOrLength );
   }
   else if (p == TheOutputDir)
   {
      p_outputDir.Clear ();
      if (sizeOrLength > 0) p_outputDir.Reserve (sizeOrLength);
   }
   else if ( p == TheCAOutputExtensionParameter )
   {
      p_outputExtension.Clear();
      if ( sizeOrLength > 0 )
         p_outputExtension.Reserve( sizeOrLength );
   }
   else if (p == ThePrefix)
   {
      p_prefix.Clear ();
      if (sizeOrLength > 0) p_prefix.Reserve (sizeOrLength);
   }
   else if (p == ThePostfix)
   {
      p_postfix.Clear ();
      if (sizeOrLength > 0) p_postfix.Reserve (sizeOrLength);
   }
   else if (p == TheSubtractFile)
   {
      p_subtractFile.Clear ();
      if (sizeOrLength > 0) p_subtractFile.Reserve (sizeOrLength);
   }
   else
      return false;

   return true;
}

size_type CometAlignmentInstance::ParameterLength (const MetaParameter* p, size_type tableRow) const
{
   if (p == TheTargetFrames) return p_targetFrames.Length ();
   if (p == TheTargetFramePath) return p_targetFrames[tableRow].path.Length ();
   if (p == TheTargetFrameDate) return p_targetFrames[tableRow].date.Length ();
   if (p == TheDrizzlePath) return p_targetFrames[tableRow].drzPath.Length ();
   if (p == TheCAInputHintsParameter ) return p_inputHints.Length();
   if (p == TheCAOutputHintsParameter ) return p_outputHints.Length();
   if (p == TheOutputDir) return p_outputDir.Length ();
   if (p == TheCAOutputExtensionParameter ) return p_outputExtension.Length();
   if (p == ThePrefix) return p_prefix.Length ();
   if (p == ThePostfix) return p_postfix.Length ();
   if (p == TheSubtractFile) return p_subtractFile.Length ();
   return 0;
}

// ----------------------------------------------------------------------------

} // pcl

// ****************************************************************************
// EOF CometAlignmentInstance.cpp - Released 2015/03/04 19:50:08 UTC
