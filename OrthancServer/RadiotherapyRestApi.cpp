/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2013 Medical Physics Department, CHU of Liege,
 * Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * In addition, as a special exception, the copyright holders of this
 * program give permission to link the code of its release with the
 * OpenSSL project's "OpenSSL" library (or with modified versions of it
 * that use the same license as the "OpenSSL" library), and distribute
 * the linked executables. You must obey the GNU General Public License
 * in all respects for all of the code used other than "OpenSSL". If you
 * modify file(s) with this exception, you may extend this exception to
 * your version of the file(s), but you are not obligated to do so. If
 * you do not wish to do so, delete this exception statement from your
 * version. If you delete this exception statement from all source files
 * in the program, then also delete it here.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/


#include "RadiotherapyRestApi.h"

#include "ServerToolbox.h"

#define RETRIEVE_CONTEXT(call)                          \
  OrthancRestApi& contextApi =                          \
    dynamic_cast<OrthancRestApi&>(call.GetContext());   \
  ServerContext& context = contextApi.GetContext()


// DICOM tags for RT-STRUCT

#define REFERENCED_STUDY_SEQUENCE "0008,1110"
#define REFERENCED_SOP_INSTANCE_UID "0008,1155"
#define FRAME_OF_REFERENCE_UID "0020,0052"
#define REFERENCED_FRAME_OF_REFERENCE_SEQUENCE "3006,0010"
#define STRUCTURE_SET_ROI_SEQUENCE "3006,0020"
#define ROI_NUMBER "3006,0022"
#define ROI_NAME "3006,0026"
#define ROI_GENERATION_ALGORITHM "3006,0036"
#define ROI_CONTOUR_SEQUENCE "3006,0039"
#define REFERENCED_ROI_NUMBER "3006,0084"
#define ROI_DISPLAY_COLOR "3006,002a"
#define CONTOUR_SEQUENCE "3006,0040"
#define CONTOUR_IMAGE_SEQUENCE "3006,0016"
#define CONTOUR_GEOMETRIC_TYPE "3006,0042"
#define NUMBER_OF_CONTOUR_POINTS "3006,0046"
#define CONTOUR_DATA "3006,0050"


namespace Orthanc
{
  static bool CheckSeriesModality(Json::Value& study,
                                  Json::Value& series,
                                  Json::Value& content,
                                  ServerContext& context,
                                  const std::string& seriesId,
                                  const std::string& modality)
  {
    if (!context.GetIndex().LookupResource(series, seriesId, ResourceType_Series))
    {
      return false;
    }

    // Retrieve the parent study
    std::string studyId = series["ParentStudy"].asString();
    if (!context.GetIndex().LookupResource(study, studyId, ResourceType_Study))
    {
      return false;
    }

    // Check the modality and that there is a single instance inside the series
    if (!series["MainDicomTags"].isMember("Modality") ||
        series["MainDicomTags"]["Modality"].asString() != modality ||
        series["Instances"].size() != 1)
    {
      return false;
    }

    // Retrieve the instance data
    std::string instanceId = series["Instances"][0].asString();

    Json::Value info;
    context.ReadJson(content, instanceId);

    return true;
  }


  static bool GetRtStructuresInfo(Json::Value& study,
                                  Json::Value& series,
                                  Json::Value& content,
                                  std::string& frameOfReference,
                                  ServerContext& context,
                                  const std::string& seriesId)
  {
    if (!CheckSeriesModality(study, series, content, context, seriesId, "RTSTRUCT"))
    {
      return false;
    }

    // Check that the "ReferencedStudySequence" is the same as the parent study.
    if (!content.isMember(REFERENCED_STUDY_SEQUENCE) ||
        content[REFERENCED_STUDY_SEQUENCE]["Value"].size() != 1 ||
        !content[REFERENCED_STUDY_SEQUENCE]["Value"][0].isMember(REFERENCED_SOP_INSTANCE_UID) ||
        content[REFERENCED_STUDY_SEQUENCE]["Value"][0][REFERENCED_SOP_INSTANCE_UID]["Value"].asString() != 
        study["MainDicomTags"]["StudyInstanceUID"].asString())
    {
      return false;
    }

    // Lookup for the frame of reference. Orthanc does not support
    // RTSTRUCT with multiple frames of reference.
    if (!content.isMember(REFERENCED_FRAME_OF_REFERENCE_SEQUENCE) ||
        content[REFERENCED_FRAME_OF_REFERENCE_SEQUENCE]["Value"].size() != 1 ||
        !content[REFERENCED_FRAME_OF_REFERENCE_SEQUENCE]["Value"][0].isMember(FRAME_OF_REFERENCE_UID))
    {
      return false;
    }

    frameOfReference = content[REFERENCED_FRAME_OF_REFERENCE_SEQUENCE]["Value"][0][FRAME_OF_REFERENCE_UID]["Value"].asString();

    return true;
  }


  static void GetRtStructuresInfo(RestApi::GetCall& call)
  {
    RETRIEVE_CONTEXT(call);

    Json::Value study, series, content;
    std::string frameOfReference;
    if (GetRtStructuresInfo(study, series, content, frameOfReference, context, call.GetUriComponent("id", "")))
    {
      Json::Value result;

      result["Study"] = study["ID"];


      // Lookup the series with the same frame of reference inside this study
      result["RelatedSeries"] = Json::arrayValue;

      for (Json::Value::ArrayIndex i = 0; i < study["Series"].size(); i++)
      {
        Json::Value otherSeries;
        if (context.GetIndex().LookupResource(otherSeries, study["Series"][i].asString(), ResourceType_Series) &&
            otherSeries["Instances"].size() > 0)
        {
          Json::Value info;
          context.ReadJson(info, otherSeries["Instances"][0].asString());

          if (info.isMember(FRAME_OF_REFERENCE_UID))
          {
            result["RelatedSeries"].append(study["Series"][i].asString());
          }
        }
      }


      call.GetOutput().AnswerJson(result);
    }
  }


  static void GetRtStructuresListOfROIs(RestApi::GetCall& call)
  {
    RETRIEVE_CONTEXT(call);

    Json::Value study, series, content;
    std::string frameOfReference;
    if (GetRtStructuresInfo(study, series, content, frameOfReference, context, call.GetUriComponent("id", "")))
    {
      Json::Value result(Json::arrayValue);

      if (content.isMember(STRUCTURE_SET_ROI_SEQUENCE))
      {
        for (Json::Value::ArrayIndex i = 0; i < content[STRUCTURE_SET_ROI_SEQUENCE]["Value"].size(); i++)
        {
          if (content[STRUCTURE_SET_ROI_SEQUENCE]["Value"][i].isMember(ROI_NUMBER))
          {
            result.append(content[STRUCTURE_SET_ROI_SEQUENCE]["Value"][i][ROI_NUMBER]["Value"].asString());
          }
        }
      }

      call.GetOutput().AnswerJson(result);
    }
  }


  static void GetRtStructuresROI(RestApi::GetCall& call)
  {
    RETRIEVE_CONTEXT(call);

    Json::Value study, series, content;
    std::string frameOfReference;
    if (GetRtStructuresInfo(study, series, content, frameOfReference, context, call.GetUriComponent("id", "")))
    {
      if (content.isMember(STRUCTURE_SET_ROI_SEQUENCE) &&
          content.isMember(ROI_CONTOUR_SEQUENCE))
      {
        Json::Value result;

        bool found = false;
        for (Json::Value::ArrayIndex i = 0; i < content[STRUCTURE_SET_ROI_SEQUENCE]["Value"].size(); i++)
        {
          const Json::Value& roi = content[STRUCTURE_SET_ROI_SEQUENCE]["Value"][i];

          if (roi.isMember(ROI_NUMBER) &&
              roi.isMember(ROI_NAME) &&
              roi[ROI_NUMBER]["Value"].asString() == call.GetUriComponent("roi", ""))
          {
            result["Number"] = call.GetUriComponent("roi", "");
            result["Name"] = roi[ROI_NAME]["Value"].asString();
            result["GenerationAlgorithm"] = roi[ROI_GENERATION_ALGORITHM]["Value"].asString();
            found = true;
          }
        }

        if (!found)
        {
          return;
        }

        found = false;

        boost::mutex::scoped_lock lock(context.GetDicomFileMutex());
        ParsedDicomFile& dicom = context.GetDicomFile(series["Instances"][0].asString());

        for (Json::Value::ArrayIndex i = 0; i < content[ROI_CONTOUR_SEQUENCE]["Value"].size(); i++)
        {
          const Json::Value& contour = content[ROI_CONTOUR_SEQUENCE]["Value"][i];

          if (contour.isMember(REFERENCED_ROI_NUMBER) &&
              contour.isMember(ROI_DISPLAY_COLOR) &&
              contour.isMember(CONTOUR_SEQUENCE) &&
              contour[REFERENCED_ROI_NUMBER]["Value"].asString() == call.GetUriComponent("roi", ""))
          {
            std::vector<std::string> color;
            Toolbox::Split(color, contour[ROI_DISPLAY_COLOR]["Value"].asString(), '\\');

            result["Points"] = Json::objectValue;
            result["ClosedPlanar"] = Json::objectValue;
            result["DisplayColor"] = Json::arrayValue;
            for (size_t k = 0; k < color.size(); k++)
            {
              result["DisplayColor"].append(boost::lexical_cast<int>(color[k]));
            }

            for (Json::Value::ArrayIndex j = 0; j < contour[CONTOUR_SEQUENCE]["Value"].size(); j++)
            {
              const Json::Value& contourSequence = contour[CONTOUR_SEQUENCE]["Value"][j];

              if (contourSequence.isMember(CONTOUR_IMAGE_SEQUENCE) &&
                  contourSequence.isMember(CONTOUR_GEOMETRIC_TYPE) &&
                  contourSequence.isMember(NUMBER_OF_CONTOUR_POINTS) &&
                  contourSequence.isMember(CONTOUR_DATA) &&
                  contourSequence[CONTOUR_IMAGE_SEQUENCE]["Value"].size() == 1 &&
                  contourSequence[CONTOUR_IMAGE_SEQUENCE]["Value"][0].isMember(REFERENCED_SOP_INSTANCE_UID))
              {
                const std::string type = contourSequence[CONTOUR_GEOMETRIC_TYPE]["Value"].asString();
                if (type != "POINT" && type != "CLOSED_PLANAR")
                {
                  continue;
                }

                const std::string uid = (contourSequence[CONTOUR_IMAGE_SEQUENCE]["Value"][0]
                                         [REFERENCED_SOP_INSTANCE_UID]["Value"].asString());

                std::list<std::string> instance;
                context.GetIndex().LookupTagValue(instance, DICOM_TAG_SOP_INSTANCE_UID, uid);
                if (instance.size() != 1)
                {
                  continue;
                }

                unsigned int countPoints = boost::lexical_cast<unsigned int>
                  (contourSequence[NUMBER_OF_CONTOUR_POINTS]["Value"].asString());
                if (countPoints <= 0)
                {
                  continue;
                }

                ParsedDicomFile::SequencePath path;
                path.push_back(std::make_pair(DicomTag(0x3006, 0x0039 /* ROIContourSequence */), i));
                path.push_back(std::make_pair(DicomTag(0x3006, 0x0040 /* ContourSequence */), j));
                
                std::string contourData;
                dicom.GetTagValue(contourData, path, DicomTag(0x3006, 0x0050 /* ContourData */));

                std::vector<std::string> points;
                Toolbox::Split(points, contourData, '\\');

                Json::Value* target;
                Json::Value item = Json::arrayValue;

                if (type == "POINT" && 
                    countPoints == 1 && 
                    points.size() == 3)
                {
                  target = &result["Points"];
                  item.append(boost::lexical_cast<float>(points[0]));
                  item.append(boost::lexical_cast<float>(points[1]));
                  item.append(boost::lexical_cast<float>(points[2]));
                }
                else if (type == "CLOSED_PLANAR" &&
                         points.size() == 3 * countPoints)
                {
                  target = &result["ClosedPlanar"];
                  for (size_t k = 0; k < countPoints; k++)
                  {
                    Json::Value p = Json::arrayValue;
                    p.append(boost::lexical_cast<float>(points[3 * k]));
                    p.append(boost::lexical_cast<float>(points[3 * k + 1]));
                    p.append(boost::lexical_cast<float>(points[3 * k + 2]));
                    item.append(p);
                  }
                }
                else
                {
                  continue;
                }
              
                if (!target->isMember(instance.front()))
                {
                  (*target) [instance.front()] = Json::arrayValue;
                }

                (*target) [instance.front()].append(item);
              }                  
            }

            found = true;
          }
        }

        if (found)
        {
          call.GetOutput().AnswerJson(result);
        }
      }
    }
  }


  RadiotherapyRestApi::RadiotherapyRestApi(ServerContext& context) : OrthancRestApi(context)
  {
    Register("/series/{id}/rt-structures", GetRtStructuresInfo);
    Register("/series/{id}/rt-structures/roi", GetRtStructuresListOfROIs);
    Register("/series/{id}/rt-structures/roi/{roi}", GetRtStructuresROI);
  }

}


//  curl http://localhost:8042/series/0b9e2bb2-605a59aa-f27c0260-9cc4faf6-9d8bf457/rt-structures
//  curl http://localhost:8042/series/ef041e6b-c855e775-f7e0f7fe-dc3c17dc-533cb8c5/rt-structures
