#include "stdafx.h"
#include <sprk_ops.h>
#include <iostream>
#include <string>
#include <algorithm>
#include "ELMCAD.h"
#include ".\LSG\CurvatureAnalyze.h"
#include "ELMCrvCurvatureDialog.h"
#include "CELMGeomUtil.h"
#include ".\LSG\AnaUtil.h"

#define COPY(dest,src,size_) if(size_ != 0)	{\
		size_t uTempSize = dest.size();\
		dest.resize(uTempSize + size_);\
		std::copy(src ,src+ size_, dest.begin() + uTempSize);\
		src+= size_; \
		size_ = 0;\
	}

#define COPY_(dest,src,size_) /*if(size_)*/ {\
		size_t uTempSize = dest.size();\
		dest.resize(uTempSize + size_);\
		std::copy(src,src+size_, dest.begin() + uTempSize);\
	}

CurvatureAnalyze::CurvatureAnalyze(CELMCADView *in_view, HPS::Exchange::CADModel const & in_cad_model, HPS::MouseButtons button, HPS::ModifierKeys modifiers)
	 : Operator(button, modifiers)
     , elmview(in_view)

{
	// get unit
	m_cad_model = elmview->GetDocument()->GetCADModel();
	if (HPS::Type::None != m_cad_model.Type())
	{
		HPS::Metadata units = m_cad_model.GetMetadata("Units");
		HPS::StringMetadata string_units(units);
		m_unit = string_units.GetValue();
	}
	
	// Get Highlight
	m_highlight_options = HPS::HighlightOptionsKit::GetDefault();

	elmWinkey = elmview->GetCanvas().GetWindowKey();
	elmWinkey.GetSelectionOptionsControl().SetProximity(0.1f);

	curCanvas = elmview->GetCanvas();
	myAnaSeg = elmview->GetCanvas().GetFrontView().GetAttachedModel().GetSegmentKey();
	RICompoSeg = myAnaSeg.Subsegment("RICompo_SEGMENT");

	//Set SHell segment Attribute
	//anaDrwAttributeKit.SetOverlay(HPS::Drawing::Overlay::Default);
	//myAnaSeg.SetDrawingAttribute(anaDrwAttributeKit);

	anaVisibiityKit.SetMarkers(true);
	anaMaterialKit.SetMarkerColor(HPS::RGBColor(1, 0, 0));
	anaMaterialKit.SetTextColor(HPS::RGBColor(0, 1, 0));
	RIMaterialKit.SetLineColor(HPS::RGBColor(1, 1, 1));
	anaMarkerAttKit.SetSymbol("SolidCircle");
	anaMarkerAttKit.SetSize(3.0f);
	anaShellTextKit.SetSize(15.0f, HPS::Text::SizeUnits::Pixels);
	anaMaterialKit.SetMarkerColor(HPS::RGBColor(1, 0, 0));
	myAnaSeg.SetMaterialMapping(anaMaterialKit);
	myAnaSeg.SetTextAttribute(anaShellTextKit);

	RICompoSeg.SetMaterialMapping(RIMaterialKit);
	RICompoSeg.SetMarkerAttribute(anaMarkerAttKit);
	RICompoSeg.SetVisibility(anaVisibiityKit);


	Is_Shell_Ana = _T("");

//   Create Modaless Dialog (Curve)
	pDlg = new ELMCrvCurvatureDialog(in_view, in_cad_model, (CWnd*)elmview);
	pDlg->Create(IDD_CRV_CURVATURE_DIALOG);
// Create Modaless Dialog (Surface & Draft Analysis)




}

CurvatureAnalyze::~CurvatureAnalyze()
{
	if (!m_preselected_key.Empty())
		UnSelection();

	HPS::HighlightOptionsKit::GetDefault();
	HPS::Selection::Sorting::Default;
	HPS::SelectabilityKit selectability;
	selectability.UnsetEverything();
	HPS::SelectionOptionsKit Sel_Option;
	Sel_Option.SetProximity(0.2f);
	myAnaSeg.UnsetDrawingAttribute();
	myAnaSeg.Flush(HPS::Search::Type::Text, HPS::Search::Space::SegmentOnly);
	myAnaSeg.Flush(HPS::Search::Type::Marker, HPS::Search::Space::SegmentOnly);
	myAnaSeg.UnsetDrawingAttribute();
	RICompoSeg.Flush(HPS::Search::Type::Text, HPS::Search::Space::SegmentOnly);
	RICompoSeg.Flush(HPS::Search::Type::Line, HPS::Search::Space::SegmentOnly);
}

bool CurvatureAnalyze::OnMouseDown(HPS::MouseState const &in_state)
{
/*
	// Define Dump file (Indices value)
	std::ofstream DumpIndicesFile;
	DumpIndicesFile.open("D:\\Temp\\IndicesVal.csv");
*/
	//Selection
	HPS::SelectionOptionsKit selection_options;
	selection_options.SetLevel(HPS::Selection::Level::Subentity).SetSorting(true); // Subentity
	HPS::SelectionResults selection_results;

	size_t number_of_selected_items = in_state.GetEventSource().GetSelectionControl().SelectByPoint(in_state.GetLocation(), selection_options, selection_results);
	
	if (number_of_selected_items = 1)
	{
		HPS::SelectionResultsIterator it = selection_results.GetIterator();

		// Get Key
		HPS::Key selected_key;
		it.GetItem().ShowSelectedItem(selected_key);

		if (selected_key.HasOwner())
			HPS::SegmentKey thisOwnerSeg = selected_key.Owner();

		HPS::Type type = selected_key.Type();
		HPS::Exchange::Component exComp = m_cad_model.GetComponentFromKey(selected_key);
		HPS::Component::ComponentType comptype = exComp.GetComponentType();
		float model_Scale = ELMGeomUtils::GetModelScale(exComp);
		if (model_Scale < 0.0001)
			model_Scale = 1.0;
		HPS::ComponentArray owners = exComp.GetOwners();

		if ((HPS::Component::ComponentType::ExchangeTopoFace == comptype) && (HPS::Type::ShellKey == type))
		{
			Is_Shell_Ana = _T("SHELL");
			// Get selected TopoShell Info
			myAnaSeg.Flush(HPS::Search::Type::Text, HPS::Search::Space::SegmentOnly);
			A3DStatus iret;

			while (owners[0].GetComponentType() != HPS::Exchange::Component::ComponentType::ExchangeTopoShell)
				owners = owners[0].GetOwners();

			HPS::Exchange::Component TopoShellComp = owners[0];
			A3DTopoShell *pTopoShell = TopoShellComp.GetExchangeEntity();

			// Get number of Faces & Pointers
			A3DTopoShellData sTopoShell;
			A3D_INITIALIZE_DATA(A3DTopoShellData, sTopoShell);
			if (A3D_SUCCESS != A3DTopoShellGet(pTopoShell, &sTopoShell))
			{
				AfxMessageBox(_T("A3DTopoShell Problem..."), MB_SETFOREGROUND + MB_ICONINFORMATION + MB_OK);
				return false;
			}

			//Get RiBrepModel
			HPS::ComponentArray owners = TopoShellComp.GetOwners();

			while (owners[0].GetComponentType() != HPS::Exchange::Component::ComponentType::ExchangeRIBRepModel)
				owners = owners[0].GetOwners();

			HPS::Exchange::Component RIBRepModelComp = owners[0];
			A3DRiBrepModel *pRiBrepModel = RIBRepModelComp.GetExchangeEntity();

            // Re-Tessellation
			A3DRWParamsTessellationData tessellationParameters;
			A3D_INITIALIZE_DATA(A3DRWParamsTessellationData, tessellationParameters);
			//tessellationParameters.m_bAccurateTessellation = A3D_TRUE;
			tessellationParameters.m_eTessellationLevelOfDetail = kA3DTessLODExtraHigh;//kA3DTessLODExtraHigh;	//kA3DTessLODMedium
			tessellationParameters.m_bAccurateTessellationWithGrid = A3D_TRUE;
			tessellationParameters.m_dAccurateTessellationWithGridMaximumStitchLength = 0.01f;
			tessellationParameters.m_bAccurateSurfaceCurvatures = A3D_TRUE;
			tessellationParameters.m_bKeepUVPoints = A3D_TRUE;

			iret = A3DRiRepresentationItemComputeTessellation((A3DRiRepresentationItem *)pRiBrepModel, &tessellationParameters);
			if (iret != A3D_SUCCESS)
				AfxMessageBox(_T("A3DRiRepresentationItemComputeTessellation Problem..."), MB_SETFOREGROUND + MB_ICONINFORMATION + MB_OK);

			//HPS::Exchange::ReloadNotifier notifier = TopoShellComp.Reload();
			//notifier.Wait();
			//curCanvas.Update();

			A3DRiRepresentationItemData sRiData;
			A3D_INITIALIZE_DATA(A3DRiRepresentationItemData, sRiData);
			iret = A3DRiRepresentationItemGet(pRiBrepModel, &sRiData);

			A3DTess3D* pTess3D = sRiData.m_pTessBase;

			// TessBase Data
			A3DTessBaseData TessBaseData;
			A3D_INITIALIZE_DATA(A3DTessBaseData, TessBaseData);
			A3DTessBaseGet(pTess3D, &TessBaseData);

			// Get color
			A3DRootBaseWithGraphicsData sBaseWithGraphicsData;
			A3D_INITIALIZE_DATA(A3DRootBaseWithGraphicsData, sBaseWithGraphicsData);
			A3DRootBaseWithGraphicsGet(pRiBrepModel, &sBaseWithGraphicsData);

			// Tess3DData
			A3DTess3DData Tess3DData;
			A3D_INITIALIZE_DATA(A3DTess3DData, Tess3DData);
			if (A3D_SUCCESS != A3DTess3DGet(pTess3D, &Tess3DData))
				AfxMessageBox(_T("A3DTess3DGet Problem..."), MB_SETFOREGROUND + MB_ICONINFORMATION + MB_OK);

			size_t Num_Faces = Tess3DData.m_uiFaceTessSize;

			// Set indices array of Node Point & Normal
			std::vector<A3DUns32> norm_Indices;
			std::vector<A3DUns32> textual_Indices;
			std::vector<A3DUns32> point_Indices;
			std::vector<double> ana_Rad_Val_AllFace;

			double min_Rad = 999999.0, max_Rad = 0.0;
			// Loop faces
			for (size_t i = 0; i < Num_Faces; i++)
			{
				A3DTopoFace *pFace = sTopoShell.m_ppFaces[i];
				A3DTopoFaceData sTopoFaceData;
				A3D_INITIALIZE_DATA(A3DTopoFaceData, sTopoFaceData);
				if (A3D_SUCCESS != A3DTopoFaceGet(pFace, &sTopoFaceData))
				{
					AfxMessageBox(_T("A3DTopoFaceGet Problem..."), MB_SETFOREGROUND + MB_ICONINFORMATION + MB_OK);
					return false; break;
				}

				A3DSurfBase* pSurfBase = sTopoFaceData.m_pSurface;
				A3DTessFaceData FaceTessData = Tess3DData.m_psFaceTessData[i];


				size_t idevide;
				// Get Triangle index Data
				if (FaceTessData.m_usUsedEntitiesFlags == kA3DTessFaceDataTriangle)
				{
					IndicesPerFaceAsTriangle_2(i, Tess3DData, norm_Indices, point_Indices);
					idevide = 6;
				}
				else if (FaceTessData.m_usUsedEntitiesFlags == kA3DTessFaceDataTriangleTextured)
				{
					IndicesPerFaceAsTriangle_512(i, Tess3DData, norm_Indices, textual_Indices, point_Indices);
					idevide = 9;
				}

				std::vector<double> ana_Rad_Val;

				size_t  num_Index1 = norm_Indices.size();
				size_t  num_Index2 = textual_Indices.size();
				size_t  num_Index3 = point_Indices.size();

				unsigned  pointIndicies1, pointIndicies2, pointIndicies3;
				double  ana_Result[5], uRadii, vRadii;

				char cInt2Txt[64] = { 0, };
				size_t iStart = FaceTessData.m_uiStartTriangulated / idevide;
				// Display Node Point with index
				for (size_t k = iStart; k < point_Indices.size() / 3; k++)
				{	
					pointIndicies1 = point_Indices[k * 3]; pointIndicies2 = point_Indices[(k * 3) + 1]; pointIndicies3 = point_Indices[(k * 3) + 2];
					// Get Parameter in One Triangle
					for (size_t oneTri = iStart; oneTri < iStart + 3; oneTri++)
					{
						A3DVector2dData pUVparam;
						A3D_INITIALIZE_DATA(A3DVector2dData, pUVparam);
						HPS::Point NodePoint1, NodePoint2, NodePoint3;
						unsigned pt_Index;
						if (oneTri == iStart)
							pt_Index =pointIndicies1;
						else if (oneTri == iStart + 1)
							pt_Index = pointIndicies2;
						else if (oneTri == iStart + 2)
							pt_Index = pointIndicies3;

						HPS::Point m_pdCoord, oSurfPt;
						m_pdCoord.x = TessBaseData.m_pdCoords[pt_Index];
						m_pdCoord.y = TessBaseData.m_pdCoords[pt_Index + 1];
						m_pdCoord.z = TessBaseData.m_pdCoords[pt_Index + 2];

						//myAnaSeg.InsertMarker(HPS::Point(m_pdCoord.x, m_pdCoord.y, m_pdCoord.z));
						//sprintf_s(cInt2Txt, "%d", pt_Index);
						//myAnaSeg.InsertText(HPS::Point(m_pdCoord.x, m_pdCoord.y, m_pdCoord.z), cInt2Txt);

						AnaUtil::GetSurfRadii(pSurfBase, m_pdCoord, model_Scale, ana_Result);
						uRadii = ana_Result[3]; vRadii = ana_Result[4];

						double uvMinRad, uvMaxRad;
						if (uRadii > vRadii)
						{
							ana_Rad_Val.push_back(vRadii);
							ana_Rad_Val_AllFace.push_back(vRadii);
							uvMinRad = vRadii;
							uvMaxRad = uRadii;
						}
						else
						{
							ana_Rad_Val.push_back(uRadii);
							ana_Rad_Val_AllFace.push_back(uRadii);
							uvMinRad = uRadii;
							uvMaxRad = vRadii;
						}

						// Get Min Max Val
						if (max_Rad <= uvMaxRad)
						{
							max_Rad = uvMaxRad;
							max_Pos = m_pdCoord;
						}

						if (min_Rad >= uvMinRad)
						{
							min_Rad = uvMinRad;
							min_Pos = m_pdCoord;
						}
					} // ------------------ end of for (one Triangle)
/*
					// Write Dump file
					char Buff[256];
					sprintf_s(Buff, 256, "%d,%d,%d,%d,%d,%d,%d,%d,%d\n", 
						normIndicies1, normIndicies2, normIndicies3, 
						textualIndicies1, textualIndicies2, textualIndicies3, 
						pointIndicies1, pointIndicies2, pointIndicies3);

					int Buffsize = strlen(Buff);
					DumpIndicesFile.write(Buff, Buffsize);
*/
				} // ----------------- end of for (Display Node Point with index)
				min_Radii = min_Rad; max_Radii = max_Rad;

			} // ------------------ end of for (nFaces)
			MinMaxSeg = myAnaSeg.Subsegment("MinMax_SEGMENT");
			HPS::MaterialMappingKit MinMaxMaterialKit;
			MinMaxMaterialKit.SetMarkerColor(HPS::RGBColor(1, 1, 0));
			MinMaxSeg.SetMaterialMapping(MinMaxMaterialKit);
			HPS::MarkerAttributeKit MinMaxMarkerAttKit;
			MinMaxMarkerAttKit.SetSize(0.3f);
			MinMaxSeg.SetMarkerAttribute(MinMaxMarkerAttKit);
			MinMaxSeg.InsertMarker(HPS::Point(max_Pos.x, max_Pos.y, max_Pos.z));
			MinMaxSeg.InsertMarker(HPS::Point(min_Pos.x, min_Pos.y, min_Pos.z));
/*
			CString msg;
			msg.Format(_T("max_Radii : %.5f, min_Radii : %.5f"), max_Radii, min_Radii);
			//AfxMessageBox(msg, MB_SETFOREGROUND + MB_ICONINFORMATION + MB_OK);
			CT2CA pszConvertedAnsiString(msg);
			std::string myStr(pszConvertedAnsiString);
			std::cout << (std::string)myStr << std::endl;
*/
			// Set RGB Vertices Color
			size_t nTriNodeSize = ana_Rad_Val_AllFace.size();
			A3DUns8 *color_store_Arr;
			color_store_Arr = new A3DUns8[nTriNodeSize * 3];

			for (size_t icol = 0; icol < nTriNodeSize; icol++)
			{
				double dRad = ana_Rad_Val_AllFace[icol]; double dCol[3] = { 0.5, 0.5, 0.5 };
				AnaUtil::AnalyzeColor(dRad, dCol);
				color_store_Arr[icol * 3] = (A3DUns8)(255.0 * dCol[0]);
				color_store_Arr[icol * 3 + 1] = (A3DUns8)(255.0 * dCol[1]);
				color_store_Arr[icol * 3 + 2] = (A3DUns8)(255.0 * dCol[2]);
			}
			A3DTessFaceData *pTessFaceData = Tess3DData.m_psFaceTessData;
			pTessFaceData->m_uiRGBAVerticesSize = nTriNodeSize * 3;
			pTessFaceData->m_pucRGBAVertices = color_store_Arr;

			// ++++++++++++++++++++++  Create New Shell with Color  ++++++++++++++++++++++

			A3DTessBaseData NewTessBaseData;
			A3D_INITIALIZE_DATA(A3DTessBaseData, NewTessBaseData);

			NewTessBaseData.m_uiCoordSize = TessBaseData.m_uiCoordSize;
			NewTessBaseData.m_pdCoords = (A3DDouble*)malloc(NewTessBaseData.m_uiCoordSize * sizeof(A3DDouble));
			NewTessBaseData.m_pdCoords = (A3DDouble*)memcpy(NewTessBaseData.m_pdCoords, TessBaseData.m_pdCoords, TessBaseData.m_uiCoordSize * sizeof(A3DDouble));

			char cInt2Txt[64] = { 0, };
			A3DTess3DData NewTess3DData;
			A3D_INITIALIZE_DATA(A3DTess3DData, NewTess3DData);

			NewTess3DData.m_uiNormalSize = Tess3DData.m_uiNormalSize;
			NewTess3DData.m_pdNormals = (A3DDouble*)malloc(NewTess3DData.m_uiNormalSize * sizeof(A3DDouble));
			NewTess3DData.m_pdNormals = (A3DDouble*)memcpy(NewTess3DData.m_pdNormals, Tess3DData.m_pdNormals, Tess3DData.m_uiNormalSize * sizeof(A3DDouble));

			A3DTessFaceData NewTessFaceData;
			A3D_INITIALIZE_DATA(A3DTessFaceData, NewTessFaceData);

			NewTessFaceData.m_usUsedEntitiesFlags = kA3DTessFaceDataTriangle;
			NewTess3DData.m_uiTriangulatedIndexSize = nTriNodeSize * 2;
			NewTess3DData.m_puiTriangulatedIndexes = (A3DUns32*)malloc(NewTess3DData.m_uiTriangulatedIndexSize * sizeof(A3DUns32));

			NewTessFaceData.m_uiSizesTriangulatedSize = 1;
			NewTessFaceData.m_puiSizesTriangulated = (A3DUns32*)malloc(NewTessFaceData.m_uiSizesTriangulatedSize * sizeof(A3DUns32));
			NewTessFaceData.m_puiSizesTriangulated[0] = nTriNodeSize / 3;
			
			for (size_t index_num = 0; index_num < nTriNodeSize / 3; index_num++)
			{
				NewTess3DData.m_puiTriangulatedIndexes[index_num * 6 + 0] = norm_Indices[index_num*3];
				NewTess3DData.m_puiTriangulatedIndexes[index_num * 6 + 1] = point_Indices[index_num*3];
				NewTess3DData.m_puiTriangulatedIndexes[index_num * 6 + 2] = norm_Indices[index_num*3 + 1];
				NewTess3DData.m_puiTriangulatedIndexes[index_num * 6 + 3] = point_Indices[index_num*3 + 1];
				NewTess3DData.m_puiTriangulatedIndexes[index_num * 6 + 4] = norm_Indices[index_num*3 + 2];
				NewTess3DData.m_puiTriangulatedIndexes[index_num * 6 + 5] = point_Indices[index_num*3 + 2];
/*
				HPS::Point m_pdCoord1, m_pdCoord2, m_pdCoord3;
				m_pdCoord1.x = NewTessBaseData.m_pdCoords[point_Indices[index_num*3]];
				m_pdCoord1.y = NewTessBaseData.m_pdCoords[point_Indices[index_num*3] + 1];
				m_pdCoord1.z = NewTessBaseData.m_pdCoords[point_Indices[index_num*3] + 2];
				char cInt2Txt0[64] = { 0, };
				myAnaSeg.InsertMarker(HPS::Point(m_pdCoord1.x, m_pdCoord1.y, m_pdCoord1.z));
				sprintf_s(cInt2Txt0, "%d", point_Indices[index_num*3]);
				myAnaSeg.InsertText(HPS::Point(m_pdCoord1.x, m_pdCoord1.y, m_pdCoord1.z), cInt2Txt0);

				m_pdCoord2.x = NewTessBaseData.m_pdCoords[point_Indices[index_num*3 + 1]];
				m_pdCoord2.y = NewTessBaseData.m_pdCoords[point_Indices[index_num*3 + 1] + 1];
				m_pdCoord2.z = NewTessBaseData.m_pdCoords[point_Indices[index_num*3 + 1] + 2];
				char cInt2Txt1[64] = { 0, };
				myAnaSeg.InsertMarker(HPS::Point(m_pdCoord2.x, m_pdCoord2.y, m_pdCoord2.z));
				sprintf_s(cInt2Txt1, "%d", point_Indices[index_num*3 + 1]);
				myAnaSeg.InsertText(HPS::Point(m_pdCoord2.x, m_pdCoord2.y, m_pdCoord2.z), cInt2Txt1);

				m_pdCoord3.x = NewTessBaseData.m_pdCoords[point_Indices[index_num*3 + 2]];
				m_pdCoord3.y = NewTessBaseData.m_pdCoords[point_Indices[index_num*3 + 2] + 1];
				m_pdCoord3.z = NewTessBaseData.m_pdCoords[point_Indices[index_num*3 + 2] + 2];
				char cInt2Txt2[64] = { 0, };
				myAnaSeg.InsertMarker(HPS::Point(m_pdCoord3.x, m_pdCoord3.y, m_pdCoord3.z));
				sprintf_s(cInt2Txt2, "%d", point_Indices[index_num*3 + 2]);
				myAnaSeg.InsertText(HPS::Point(m_pdCoord3.x, m_pdCoord3.y, m_pdCoord3.z), cInt2Txt2);
*/
			}

			NewTess3DData.m_uiFaceTessSize = 1;
			NewTess3DData.m_psFaceTessData = &NewTessFaceData;
			NewTessFaceData.m_uiStartTriangulated = 0;
			NewTessFaceData.m_uiRGBAVerticesSize = nTriNodeSize * 3;
			NewTessFaceData.m_pucRGBAVertices = color_store_Arr;

			A3DTess3D* pNewTess3D = NULL;
			iret = A3DTess3DCreate(&NewTess3DData, &pNewTess3D);
			iret = A3DTessBaseSet(pNewTess3D, &NewTessBaseData);

			A3DRiPolyBrepModelData sPolyBrepModelData;
			A3D_INITIALIZE_DATA(A3DRiPolyBrepModelData, sPolyBrepModelData);
			sPolyBrepModelData.m_bIsClosed = FALSE;

			A3DRiRepresentationItem * pRiAnalyze = 0;

			if (A3D_SUCCESS == A3DRiPolyBrepModelCreate(&sPolyBrepModelData, &pRiAnalyze))
			{
				A3DRiRepresentationItemData sNewRiData;
				A3D_INITIALIZE_DATA(A3DRiRepresentationItemData, sNewRiData);
				sNewRiData.m_pTessBase = pNewTess3D;
				sNewRiData.m_pCoordinateSystem = sRiData.m_pCoordinateSystem;
				A3DRiRepresentationItemSet(pRiAnalyze, &sNewRiData);
			}

			// Get the parent part
			HPS::ComponentArray compArr = RIBRepModelComp.GetOwners();
			HPS::Component::ComponentType compType = compArr[0].GetComponentType();

			while (HPS::Component::ComponentType::ExchangePartDefinition != compType)
			{
				HPS::Component comp = compArr[0];
				compArr = comp.GetOwners();
				compType = compArr[0].GetComponentType();

				if (HPS::Component::ComponentType::ExchangeModelFile == compType)
					return false;
			}

			HPS::Exchange::Component exPOComp;
			exPOComp = (HPS::Exchange::Component)compArr[0];

			A3DAsmPartDefinition* pPartOrg = exPOComp.GetExchangeEntity();

			// Update org part
			A3DAsmPartDefinitionData sPartData;
			A3D_INITIALIZE_DATA(A3DAsmPartDefinitionData, sPartData);
			A3DAsmPartDefinitionGet(pPartOrg, &sPartData);

			A3DRiRepresentationItem** ppRepItems = new A3DRiRepresentationItem*[sPartData.m_uiRepItemsSize + 1];

			for (size_t ui = 0; ui < sPartData.m_uiRepItemsSize; ui++)
			{
				ppRepItems[ui] = sPartData.m_ppRepItems[ui];
			}
			ppRepItems[sPartData.m_uiRepItemsSize] = pRiAnalyze;

			sPartData.m_uiRepItemsSize += 1;
			sPartData.m_ppRepItems = ppRepItems;

			A3DStatus status = A3DAsmPartDefinitionEdit(&sPartData, pPartOrg);

			// Redraw
			HPS::Exchange::ReloadNotifier notifier = exPOComp.Reload();
			notifier.Wait();
			//m_cad_model.ActivateDefaultCapture();
			elmview->GetCanvas().Update(HPS::Window::UpdateType::Complete);

			// Release Memory for making NewTessBaseData & Color_Store_Arr
			free(NewTessBaseData.m_pdCoords);
			free(NewTess3DData.m_pdNormals);
			free(NewTess3DData.m_puiTriangulatedIndexes);
			free(NewTessFaceData.m_puiSizesTriangulated);
			free(NewTess3DData.m_pdTextureCoords);
			delete color_store_Arr;

			// Isolate Product Occurrence
			HPS::ComponentArray EMCompoArr = m_cad_model.GetAllSubcomponents(HPS::Component::ComponentType::ExchangeModelFile); //ExchangeRIPolyBRepModel
			if (EMCompoArr.size() == 1)
			{
				m_selection_component = EMCompoArr[0];
				CString PolyCompo_name = (CString)m_selection_component.GetName();
				HPS::KeyPathArray keyPathArr = HPS::Component::GetKeyPath(EMCompoArr[0]);
				compPath = m_cad_model.GetComponentPath(keyPathArr[0]);
				//compPath.Isolate(elmview->GetCanvas(),0, HPS::Component::Visibility::PreserveAll);
				compPath.Isolate(elmview->GetCanvas());
			}

			// get PolyBrepComponent
			HPS::ComponentArray polyCompoArr = m_cad_model.GetAllSubcomponents(HPS::Component::ComponentType::ExchangeRIPolyBRepModel);
			if (polyCompoArr.size() == 1)
			{
				m_selection_component = polyCompoArr[0];
				pSelPolyEntity = m_selection_component.GetExchangeEntity();
				//CString PolyCompo_name = (CString)m_selection_component.GetName();
			}

			//DumpIndicesFile.close();
		}
		else if ((HPS::Component::ComponentType::ExchangeTopoEdge == comptype) || (HPS::Component::ComponentType::ExchangeRICurve == comptype))
		{
			Is_Shell_Ana = _T("CURVE");

			pDlg->SetSelectedKey(selected_key);
			pDlg->ShowWindow(SW_SHOW);
			pDlg->calcRadius(selected_key);
		}
	}
	curCanvas.Update();

	return false;
}

bool CurvatureAnalyze::OnMouseMove(HPS::MouseState const &in_state)
{
/*
	//  Hide polyCompo
	A3DStatus istat; HPS::UTF8 myCompoName;
	HPS::ComponentArray polyCompoArr = m_cad_model.GetAllSubcomponents(HPS::Component::ComponentType::ExchangeRIPolyBRepModel);
	if (polyCompoArr.size() == 1)
	{
		m_selection_component = polyCompoArr[0];
		myCompoName = m_selection_component.GetName();
		pSelPolyEntity = m_selection_component.GetExchangeEntity();

		HPS::KeyPathArray keyPathArr = HPS::Component::GetKeyPath(m_selection_component);
		HPS::ComponentPath compPath = m_cad_model.GetComponentPath(keyPathArr[0]);
		compPath.Hide(elmview->GetCanvas());
		//istat = A3DEntityDelete(pSelPolyEntity);
		//m_selection_component.Delete();
	}
*/
	try
	{
		//HPS::HighlightOptionsKit ShellHighlightOPKit;
		//ShellHighlightOPKit.SetOverlay(HPS::Drawing::Overlay::Default);
		HPS::SelectionOptionsKit selection_options;
		selection_options.SetLevel(HPS::Selection::Level::Subentity).SetSorting(true);

		if (Is_Shell_Ana == _T("SHELL")) // Shell treat
		{
			HPS::SelectabilityKit MySelectibility;
			MySelectibility.SetEverything(HPS::Selectability::Value::Off).SetFaces(true);
			selection_options.SetProximity(0.01f);
			HPS::SelectionResults selection_Results;
			selection_Results = CurvatureAnalyze_SelOp.GetActiveSelection();
			size_t number_of_selected_items = in_state.GetEventSource().GetSelectionControl().SelectByPoint(in_state.GetLocation(), selection_options, selection_Results);

			if (number_of_selected_items > 0)
			{
				HPS::SelectionResultsIterator it = selection_Results.GetIterator();
				HPS::Key selected_key;
				it.GetItem().ShowSelectedItem(selected_key);

				HPS::KeyPath sel_path;
				HPS::SelectionItem selItem = it.GetItem();
				HPS::WorldPoint wp, wpt;
				HPS::MatrixKit m_netMatrix;

				selItem.ShowSelectionPosition(wp);

				selItem.ShowPath(sel_path);
				sel_path.ShowNetModellingMatrix(m_netMatrix);
				HPS::MatrixKit invMatrix;
				m_netMatrix.ShowInverse(invMatrix);
				HPS::Point picking_Point = invMatrix.Transform(wp);

				HPS::Point onSurfPt, pre_PT;
				pre_PT.x = 0.0; pre_PT.y = 0.0; pre_PT.z = 0.0;
				double dtmpDist = AnaUtil::distPt2(pre_PT, picking_Point);

				HPS::Type type = selected_key.Type();
				HPS::Exchange::Component exComp = m_cad_model.GetComponentFromKey(selected_key);
				HPS::Component::ComponentType comptype = exComp.GetComponentType();
				float model_Scale = AnaUtil::GetModelScale(exComp);

				A3DVector3dData Cursor_Pos;
				A3D_INITIALIZE_DATA(A3DVector3dData, Cursor_Pos);
				if (model_Scale < 0.0001)
					model_Scale = 1.0;
				Cursor_Pos.m_dX = picking_Point.x / model_Scale;
				Cursor_Pos.m_dY = picking_Point.y / model_Scale;
				Cursor_Pos.m_dZ = picking_Point.z / model_Scale;

				if ((HPS::Component::ComponentType::ExchangeTopoFace == comptype) && (HPS::Type::ShellKey == type))
				{
					//Get TopoFace Data
					HPS::Exchange::Component TopoFaceComp = exComp;
					A3DTopoFace *pTopoFace = TopoFaceComp.GetExchangeEntity();
					A3DTopoFaceData sFaceData;
					A3D_INITIALIZE_DATA(A3DTopoFaceData, sFaceData);
					A3DStatus iret = A3D_SUCCESS;
					iret = A3DTopoFaceGet(pTopoFace, &sFaceData);
					/*
									// Get Body Data
									HPS::ComponentArray shells = TopoFaceComp.GetOwners();
									HPS::Exchange::Component ShellCompo = shells[0];
									A3DTopoShell *pTopoShell = ShellCompo.GetExchangeEntity();
									HPS::ComponentArray connexes = shells[0].GetOwners();
									size_t num_entity = connexes.size();
									if (num_entity > 0)
									{
										HPS::Exchange::Component TopoConex = connexes[0];
										A3DTopoConnex *pTopoConnex = TopoConex.GetExchangeEntity();
										HPS::ComponentArray bodies = connexes[0].GetOwners();
										HPS::Exchange::Component body = bodies[0];
										A3DTopoBody *pTopoBody = body.GetExchangeEntity();
									}
					*/
					//Get Orientation
					A3DUns8 getFaceOrient = AnaUtil::getOrientation(TopoFaceComp);

					char cInt2Txt[64] = { 0, };
					double  ana_Result[5], oRadii, uRadii, vRadii;
					HPS::Point onSurfPoint, normpt_15mm, UDir_Pt, VDir_Pt;

					if (iret == A3D_SUCCESS)
					{
						A3DSurfBase* pMySurf = sFaceData.m_pSurface;
						AnaUtil::GetSurfRadii(pMySurf, picking_Point, model_Scale, ana_Result);
						onSurfPoint.x = ana_Result[0];
						onSurfPoint.y = ana_Result[1];
						onSurfPoint.z = ana_Result[2];

						int rev_Dir = 1;
						if (getFaceOrient == 0)
							rev_Dir = -1;

						normpt_15mm.x = onSurfPoint.x + ana_Result[5] * rev_Dir * 15.0;
						normpt_15mm.y = onSurfPoint.y + ana_Result[6] * rev_Dir * 15.0;
						normpt_15mm.z = onSurfPoint.z + ana_Result[7] * rev_Dir * 15.0;
						/*
											UDir_Pt.x = onSurfPoint.x + ana_Result[8] * 10.0;
											UDir_Pt.y = onSurfPoint.y + ana_Result[9] * 10.0;
											UDir_Pt.z = onSurfPoint.z + ana_Result[10] * 5.0;

											VDir_Pt.x = onSurfPoint.x + ana_Result[11] * 10.0;
											VDir_Pt.y = onSurfPoint.y + ana_Result[12] * 10.0;
											VDir_Pt.z = onSurfPoint.z + ana_Result[13] * 10.0;
						*/
						uRadii = ana_Result[3];
						vRadii = ana_Result[4];

						if (uRadii > vRadii)
							oRadii = vRadii;
						else
							oRadii = uRadii;

						// Prepare Display Radius Value
						RICompoSeg.Flush(HPS::Search::Type::Text, HPS::Search::Space::SegmentOnly);
						RICompoSeg.Flush(HPS::Search::Type::Line, HPS::Search::Space::SegmentOnly);
						// Set LeaderLine & TextBox

						RICompoSeg.InsertLine(onSurfPoint, normpt_15mm);
						if (oRadii > 1e16)
							sprintf_s(cInt2Txt, sizeof(cInt2Txt), "R:inf");
						else
							sprintf_s(cInt2Txt, "R:%.5f", oRadii);

						RICompoSeg.InsertText(HPS::Point(normpt_15mm.x, normpt_15mm.y, normpt_15mm.z), cInt2Txt);
						HPS::Point pre_PT = picking_Point;
					}
					else
					{
						AfxMessageBox(_T("+++A3DTopoFaceGet Problem..."), MB_SETFOREGROUND + MB_ICONINFORMATION + MB_OK);
						return false;
					}

				}
				elmview->GetCanvas().Update();
				//ShellHighlightOPKit.GetDefault();
			}
		}
		else if (Is_Shell_Ana == _T("CURVE")) // Curve 
		{
			elmWinkey.GetHighlightControl().UnhighlightEverything();
			HPS::SelectionOptionsKit selection_options;

			//Set Selection
			HPS::SelectionResults selection_results;
			selection_results = CurvatureAnalyze_SelOp.GetActiveSelection();

			size_t number_of_selected_items = in_state.GetEventSource().GetSelectionControl().SelectByPoint(in_state.GetLocation(), selection_options, selection_results);

			HPS::Key selected_key;
			if (number_of_selected_items > 0)
			{

				HPS::SegmentKey current_Seg, ParentSeg;
				HPS::UTF8 SegName;

				HPS::SelectionResultsIterator it = selection_results.GetIterator();

				// Get key
				it.GetItem().ShowSelectedItem(selected_key);
				current_Seg = selected_key.Up();
				ParentSeg = current_Seg.Owner();

				if (ParentSeg.Name() == "Porcu_SEGMENT")
				{
					selection_options.SetLevel(HPS::Selection::Level::Segment).SetProximity(0.1f);

					SegName = current_Seg.Name();

					if (current_Seg != m_preSelect_segKey)
					{

						if ((m_preSelect_segKey_name != SegName) && (m_preSelect_segKey_name != ""))
						{
							HPS::MaterialMappingKit RedLineKit;
							RedLineKit.SetLineColor(HPS::RGBColor(1, 0, 0));
							m_preSelect_segKey.SetMaterialMapping(RedLineKit);
							m_preSelect_segKey.Flush(HPS::Search::Type::Text, HPS::Search::Space::SegmentOnly);
						}
					}
					Display_anaCrvText(current_Seg);
					m_preSelect_segKey = current_Seg;
					m_preSelect_segKey_name = m_preSelect_segKey.Name();

					//std::cout << " Segment Name  = " << (HPS::UTF8)SegName << std::endl;
					//std::cout << " m_preSelect_segKey Name  =" << (HPS::UTF8)m_preSelect_segKey.Name() << std::endl;
				}

			} // end if (number_of_selected_items > 0)
		}
		elmview->GetCanvas().Update();
	}
	catch (...)
	{
	}
	return true;
}

void CurvatureAnalyze::Display_anaCrvText(HPS::SegmentKey in_SegKey)
{
	// Set TextKit
	HPS::TextKit anaCrvTextKit;
	anaCrvTextKit.SetSize(10, HPS::Text::SizeUnits::Pixels);
	anaCrvTextKit.SetAlignment(HPS::Text::Alignment::BottomLeft);
	anaCrvTextKit.SetColor(HPS::RGBAColor(1, 1, 1, 1));

	CString myStr = _T("");

	HPS::ByteArray oData0, oData1, oData2, oData3, oData4, oData5, oData6, oData7, oData8;
	double dRad, dKap, dsPtx, dsPty, dsPtz, dePtx, dePty, dePtz;

	in_SegKey.GetMaterialMappingControl().SetLineColor(HPS::RGBColor(0, 1, 0));

	in_SegKey.ShowUserData(0, oData0); myStr.Format(_T("%c"), oData0[0]);	// tag
	in_SegKey.ShowUserData(1, oData1); dRad = Byte2Double(oData1);	// Radii
	in_SegKey.ShowUserData(2, oData2); dKap = Byte2Double(oData2);	// Curvature

	in_SegKey.ShowUserData(3, oData3);	dsPtx = Byte2Double(oData3);// Start Pt.x
	in_SegKey.ShowUserData(4, oData4);	dsPty = Byte2Double(oData4);// Start Pt.y
	in_SegKey.ShowUserData(5, oData5);	dsPtz = Byte2Double(oData5);// Start Pt.z

	in_SegKey.ShowUserData(6, oData6);	dePtx = Byte2Double(oData6);// End Pt.x
	in_SegKey.ShowUserData(7, oData7);	dePty = Byte2Double(oData7);// End Pt.y
	in_SegKey.ShowUserData(8, oData8);	dePtz = Byte2Double(oData8);// End Pt.z

	HPS::Point sPt;
	sPt.x = dsPtx; sPt.y = dsPty; sPt.z = dsPtz;

	HPS::Point ePt;
	ePt.x = dePtx; ePt.y = dePty; ePt.z = dePtz;
	char draw_Text[64] = { 0, };


	if (myStr == _T("C"))
	{
		sprintf_s(draw_Text, "C : %.5f", dKap);
		anaCrvTextKit.SetLeaderLine(HPS::Point(ePt.x, ePt.y, ePt.z), HPS::Text::LeaderLineSpace::World);
		anaCrvTextKit.SetPosition(HPS::Point(ePt.x - 1.0f, ePt.y - 1.0f, ePt.z - 1.0f));
	}
	else
	{
		sprintf_s(draw_Text, "R : %.3f", dRad);
		anaCrvTextKit.SetLeaderLine(HPS::Point(sPt.x, sPt.y, sPt.z), HPS::Text::LeaderLineSpace::World);
		anaCrvTextKit.SetPosition(HPS::Point(sPt.x - 1.0f, sPt.y - 1.0f, sPt.z - 1.0f));
	}
	anaCrvTextKit.SetText(draw_Text);
	in_SegKey.InsertText(anaCrvTextKit);
	//in_SegKey.InsertText(HPS::Point(sPt.x - 1.0f, sPt.y - 1.0f, sPt.z - 1.0f), draw_Text);
}

void CurvatureAnalyze::UnSelection()
{

}

double CurvatureAnalyze::Byte2Double(HPS::ByteArray in_Data)
{

	double d = 0.0; char buff[64] = { 0, };
	//to_type(in_Data, d);
	//return d;

	if (in_Data.size() <= 64)
		for (size_t i = 0; i < in_Data.size(); i++) { buff[i] = in_Data[i]; }
	else
		AfxMessageBox(_T("Byte2Double Problem... Over 128 Byte Char"), MB_SETFOREGROUND + MB_ICONINFORMATION + MB_OK);

	d = atof(buff);

	return d;

}

A3DStatus CurvatureAnalyze::IndicesPerFaceAsTriangle_2(const unsigned& anaFaceIndice, const A3DTess3DData& m_sTessData,
	std::vector<A3DUns32>& anaTriangle_Normal_Indices,
	std::vector<A3DUns32>& anaTriangle_Point_Indices)
{
	A3DTessFaceData* pFaceTessData = &(m_sTessData.m_psFaceTessData[anaFaceIndice]);
	if (!pFaceTessData->m_uiSizesTriangulatedSize)
		return A3D_SUCCESS;

	A3DUns32* puiTriangulatedIndexes = m_sTessData.m_puiTriangulatedIndexes
		+ pFaceTessData->m_uiStartTriangulated;
	unsigned uiCurrentSize = 0;

	// Textured triangle One normal per vertex
	if (pFaceTessData->m_usUsedEntitiesFlags & kA3DTessFaceDataTriangle)
	{
		//(normal,{texture...},point,normal,{texture...},point,normal,{texture...},point)
		A3DUns32 uNbTriangles = pFaceTessData->m_puiSizesTriangulated[uiCurrentSize++];

		for (A3DUns32 uI = 0; uI < uNbTriangles * 3; uI++)
		{
			A3DUns32* pNormalIndice = puiTriangulatedIndexes;
			COPY_(anaTriangle_Normal_Indices, puiTriangulatedIndexes, 1);
			puiTriangulatedIndexes += 1;
			COPY_(anaTriangle_Point_Indices, puiTriangulatedIndexes, 1);
			puiTriangulatedIndexes += 1;
		}
	}
	return A3D_SUCCESS;
}

A3DStatus CurvatureAnalyze::IndicesPerFaceAsTriangle_512(const unsigned& anaFaceIndice, const A3DTess3DData& m_sTessData,
	std::vector<A3DUns32>& anaTriangle_Normal_Indices,
	std::vector<A3DUns32>& anaTriangle_Textual_Indices,
	std::vector<A3DUns32>& anaTriangle_Point_Indices)
{
	A3DTessFaceData* pFaceTessData = &(m_sTessData.m_psFaceTessData[anaFaceIndice]);
	if (!pFaceTessData->m_uiSizesTriangulatedSize)
		return A3D_SUCCESS;

	A3DUns32* puiTriangulatedIndexes = m_sTessData.m_puiTriangulatedIndexes
		+ pFaceTessData->m_uiStartTriangulated;
	unsigned uiCurrentSize = 0;

	// Textured triangle One normal per vertex
	if (pFaceTessData->m_usUsedEntitiesFlags & kA3DTessFaceDataTriangleTextured)
	{
		//(normal,{texture...},point,normal,{texture...},point,normal,{texture...},point)
		A3DUns32 uNbTriangles = pFaceTessData->m_puiSizesTriangulated[uiCurrentSize++];

		for (A3DUns32 uI = 0; uI < uNbTriangles * 3; uI++)
		{
			A3DUns32* pNormalIndice = puiTriangulatedIndexes;
			COPY_(anaTriangle_Normal_Indices, puiTriangulatedIndexes, 1);
			puiTriangulatedIndexes += 1;
			COPY_(anaTriangle_Textual_Indices, puiTriangulatedIndexes, 1);
			puiTriangulatedIndexes += 1;
			COPY_(anaTriangle_Point_Indices, puiTriangulatedIndexes, 1);
			puiTriangulatedIndexes += 1;
		}
	}

	return A3D_SUCCESS;
}