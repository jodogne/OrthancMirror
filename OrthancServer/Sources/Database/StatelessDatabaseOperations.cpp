/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2022 Osimis S.A., Belgium
 * Copyright (C) 2021-2022 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/


#include "../PrecompiledHeadersServer.h"
#include "StatelessDatabaseOperations.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "../../../OrthancFramework/Sources/DicomParsing/FromDcmtkBridge.h"
#include "../../../OrthancFramework/Sources/DicomParsing/ParsedDicomFile.h"
#include "../../../OrthancFramework/Sources/Logging.h"
#include "../../../OrthancFramework/Sources/OrthancException.h"
#include "../OrthancConfiguration.h"
#include "../Search/DatabaseLookup.h"
#include "../ServerIndexChange.h"
#include "../ServerToolbox.h"
#include "ResourcesContent.h"

#include <boost/lexical_cast.hpp>
#include <boost/thread.hpp>
#include <boost/tuple/tuple.hpp>
#include <stack>


namespace Orthanc
{
  // copy all tags from Json
  void DicomSequencesMap::FromJson(const Json::Value& value)
  {
    Json::Value::Members members = value.getMemberNames();
    for (size_t i = 0; i < members.size(); i++)
    {
      DicomTag tag = FromDcmtkBridge::ParseTag(members[i].c_str());
      sequences_[tag] = value[members[i]];
    }
  }

  // copy a subset of tags from Json
  void DicomSequencesMap::FromDicomAsJson(const Json::Value& dicomAsJson, const std::set<DicomTag>& tags)
  {
    for (std::set<DicomTag>::const_iterator it = tags.begin();
         it != tags.end(); ++it)
    {
      std::string tag = it->Format();
      if (dicomAsJson.isMember(tag))
      {
        sequences_[*it] = dicomAsJson[tag];
      }
    }
  }

  void DicomSequencesMap::ToJson(Json::Value& target, DicomToJsonFormat format) const
  {
    // add the sequences to "target"
    for (std::map<DicomTag, Json::Value>::const_iterator it = sequences_.begin();
          it != sequences_.end(); ++it)
    {
      Json::Value sequenceForConversion = Json::objectValue;
      sequenceForConversion[it->first.Format()] = it->second;

      Json::Value requestedFormatJson;
      Toolbox::SimplifyDicomAsJson(requestedFormatJson, sequenceForConversion, format);  
      
      Json::Value::Members keys = requestedFormatJson.getMemberNames();  
      for (size_t i = 0; i < keys.size(); i++)  // there should always be only one member in this JSON
      {
        target[keys[i]] = requestedFormatJson[keys[i]];
      }
    }
  }

  namespace
  {
    /**
     * Some handy templates to reduce the verbosity in the definitions
     * of the internal classes.
     **/
    
    template <typename Operations,
              typename Tuple>
    class TupleOperationsWrapper : public StatelessDatabaseOperations::IReadOnlyOperations
    {
    protected:
      Operations&   operations_;
      const Tuple&  tuple_;
    
    public:
      TupleOperationsWrapper(Operations& operations,
                             const Tuple& tuple) :
        operations_(operations),
        tuple_(tuple)
      {
      }
    
      virtual void Apply(StatelessDatabaseOperations::ReadOnlyTransaction& transaction) ORTHANC_OVERRIDE
      {
        operations_.ApplyTuple(transaction, tuple_);
      }
    };


    template <typename T1>
    class ReadOnlyOperationsT1 : public boost::noncopyable
    {
    public:
      typedef typename boost::tuple<T1>  Tuple;
      
      virtual ~ReadOnlyOperationsT1()
      {
      }

      virtual void ApplyTuple(StatelessDatabaseOperations::ReadOnlyTransaction& transaction,
                              const Tuple& tuple) = 0;

      void Apply(StatelessDatabaseOperations& index,
                 T1 t1)
      {
        const Tuple tuple(t1);
        TupleOperationsWrapper<ReadOnlyOperationsT1, Tuple> wrapper(*this, tuple);
        index.Apply(wrapper);
      }
    };


    template <typename T1,
              typename T2>
    class ReadOnlyOperationsT2 : public boost::noncopyable
    {
    public:
      typedef typename boost::tuple<T1, T2>  Tuple;
      
      virtual ~ReadOnlyOperationsT2()
      {
      }

      virtual void ApplyTuple(StatelessDatabaseOperations::ReadOnlyTransaction& transaction,
                              const Tuple& tuple) = 0;

      void Apply(StatelessDatabaseOperations& index,
                 T1 t1,
                 T2 t2)
      {
        const Tuple tuple(t1, t2);
        TupleOperationsWrapper<ReadOnlyOperationsT2, Tuple> wrapper(*this, tuple);
        index.Apply(wrapper);
      }
    };


    template <typename T1,
              typename T2,
              typename T3>
    class ReadOnlyOperationsT3 : public boost::noncopyable
    {
    public:
      typedef typename boost::tuple<T1, T2, T3>  Tuple;
      
      virtual ~ReadOnlyOperationsT3()
      {
      }

      virtual void ApplyTuple(StatelessDatabaseOperations::ReadOnlyTransaction& transaction,
                              const Tuple& tuple) = 0;

      void Apply(StatelessDatabaseOperations& index,
                 T1 t1,
                 T2 t2,
                 T3 t3)
      {
        const Tuple tuple(t1, t2, t3);
        TupleOperationsWrapper<ReadOnlyOperationsT3, Tuple> wrapper(*this, tuple);
        index.Apply(wrapper);
      }
    };


    template <typename T1,
              typename T2,
              typename T3,
              typename T4>
    class ReadOnlyOperationsT4 : public boost::noncopyable
    {
    public:
      typedef typename boost::tuple<T1, T2, T3, T4>  Tuple;
      
      virtual ~ReadOnlyOperationsT4()
      {
      }

      virtual void ApplyTuple(StatelessDatabaseOperations::ReadOnlyTransaction& transaction,
                              const Tuple& tuple) = 0;

      void Apply(StatelessDatabaseOperations& index,
                 T1 t1,
                 T2 t2,
                 T3 t3,
                 T4 t4)
      {
        const Tuple tuple(t1, t2, t3, t4);
        TupleOperationsWrapper<ReadOnlyOperationsT4, Tuple> wrapper(*this, tuple);
        index.Apply(wrapper);
      }
    };


    template <typename T1,
              typename T2,
              typename T3,
              typename T4,
              typename T5>
    class ReadOnlyOperationsT5 : public boost::noncopyable
    {
    public:
      typedef typename boost::tuple<T1, T2, T3, T4, T5>  Tuple;
      
      virtual ~ReadOnlyOperationsT5()
      {
      }

      virtual void ApplyTuple(StatelessDatabaseOperations::ReadOnlyTransaction& transaction,
                              const Tuple& tuple) = 0;

      void Apply(StatelessDatabaseOperations& index,
                 T1 t1,
                 T2 t2,
                 T3 t3,
                 T4 t4,
                 T5 t5)
      {
        const Tuple tuple(t1, t2, t3, t4, t5);
        TupleOperationsWrapper<ReadOnlyOperationsT5, Tuple> wrapper(*this, tuple);
        index.Apply(wrapper);
      }
    };


    template <typename T1,
              typename T2,
              typename T3,
              typename T4,
              typename T5,
              typename T6>
    class ReadOnlyOperationsT6 : public boost::noncopyable
    {
    public:
      typedef typename boost::tuple<T1, T2, T3, T4, T5, T6>  Tuple;
      
      virtual ~ReadOnlyOperationsT6()
      {
      }

      virtual void ApplyTuple(StatelessDatabaseOperations::ReadOnlyTransaction& transaction,
                              const Tuple& tuple) = 0;

      void Apply(StatelessDatabaseOperations& index,
                 T1 t1,
                 T2 t2,
                 T3 t3,
                 T4 t4,
                 T5 t5,
                 T6 t6)
      {
        const Tuple tuple(t1, t2, t3, t4, t5, t6);
        TupleOperationsWrapper<ReadOnlyOperationsT6, Tuple> wrapper(*this, tuple);
        index.Apply(wrapper);
      }
    };
  }


  template <typename T>
  static void FormatLog(Json::Value& target,
                        const std::list<T>& log,
                        const std::string& name,
                        bool done,
                        int64_t since,
                        bool hasLast,
                        int64_t last)
  {
    Json::Value items = Json::arrayValue;
    for (typename std::list<T>::const_iterator
           it = log.begin(); it != log.end(); ++it)
    {
      Json::Value item;
      it->Format(item);
      items.append(item);
    }

    target = Json::objectValue;
    target[name] = items;
    target["Done"] = done;

    if (!hasLast)
    {
      // Best-effort guess of the last index in the sequence
      if (log.empty())
      {
        last = since;
      }
      else
      {
        last = log.back().GetSeq();
      }
    }
    
    target["Last"] = static_cast<int>(last);
  }


  static void CopyListToVector(std::vector<std::string>& target,
                               const std::list<std::string>& source)
  {
    target.resize(source.size());

    size_t pos = 0;
    
    for (std::list<std::string>::const_iterator
           it = source.begin(); it != source.end(); ++it)
    {
      target[pos] = *it;
      pos ++;
    }      
  }


  class StatelessDatabaseOperations::MainDicomTagsRegistry : public boost::noncopyable
  {
  private:
    class TagInfo
    {
    private:
      ResourceType  level_;
      DicomTagType  type_;

    public:
      TagInfo()
      {
      }

      TagInfo(ResourceType level,
              DicomTagType type) :
        level_(level),
        type_(type)
      {
      }

      ResourceType GetLevel() const
      {
        return level_;
      }

      DicomTagType GetType() const
      {
        return type_;
      }
    };
      
    typedef std::map<DicomTag, TagInfo>   Registry;


    Registry  registry_;
      
    void LoadTags(ResourceType level)
    {
      {
        const DicomTag* tags = NULL;
        size_t size;
  
        ServerToolbox::LoadIdentifiers(tags, size, level);
  
        for (size_t i = 0; i < size; i++)
        {
          if (registry_.find(tags[i]) == registry_.end())
          {
            registry_[tags[i]] = TagInfo(level, DicomTagType_Identifier);
          }
          else
          {
            // These patient-level tags are copied in the study level
            assert(level == ResourceType_Study &&
                   (tags[i] == DICOM_TAG_PATIENT_ID ||
                    tags[i] == DICOM_TAG_PATIENT_NAME ||
                    tags[i] == DICOM_TAG_PATIENT_BIRTH_DATE));
          }
        }
      }

      {
        const std::set<DicomTag>& tags = DicomMap::GetMainDicomTags(level);

        for (std::set<DicomTag>::const_iterator
               tag = tags.begin(); tag != tags.end(); ++tag)
        {
          if (registry_.find(*tag) == registry_.end())
          {
            registry_[*tag] = TagInfo(level, DicomTagType_Main);
          }
        }
      }
    }

  public:
    MainDicomTagsRegistry()
    {
      LoadTags(ResourceType_Patient);
      LoadTags(ResourceType_Study);
      LoadTags(ResourceType_Series);
      LoadTags(ResourceType_Instance); 
    }

    void LookupTag(ResourceType& level,
                   DicomTagType& type,
                   const DicomTag& tag) const
    {
      Registry::const_iterator it = registry_.find(tag);

      if (it == registry_.end())
      {
        // Default values
        level = ResourceType_Instance;
        type = DicomTagType_Generic;
      }
      else
      {
        level = it->second.GetLevel();
        type = it->second.GetType();
      }
    }
  };


  void StatelessDatabaseOperations::ReadWriteTransaction::LogChange(int64_t internalId,
                                                                    ChangeType changeType,
                                                                    ResourceType resourceType,
                                                                    const std::string& publicId)
  {
    ServerIndexChange change(changeType, resourceType, publicId);

    if (changeType <= ChangeType_INTERNAL_LastLogged)
    {
      transaction_.LogChange(internalId, change);
    }

    GetTransactionContext().SignalChange(change);
  }


  SeriesStatus StatelessDatabaseOperations::ReadOnlyTransaction::GetSeriesStatus(int64_t id,
                                                                                 int64_t expectedNumberOfInstances)
  {
    std::list<std::string> values;
    transaction_.GetChildrenMetadata(values, id, MetadataType_Instance_IndexInSeries);

    std::set<int64_t> instances;

    for (std::list<std::string>::const_iterator
           it = values.begin(); it != values.end(); ++it)
    {
      int64_t index;

      try
      {
        index = boost::lexical_cast<int64_t>(*it);
      }
      catch (boost::bad_lexical_cast&)
      {
        return SeriesStatus_Unknown;
      }
      
      if (!(index > 0 && index <= expectedNumberOfInstances))
      {
        // Out-of-range instance index
        return SeriesStatus_Inconsistent;
      }

      if (instances.find(index) != instances.end())
      {
        // Twice the same instance index
        return SeriesStatus_Inconsistent;
      }

      instances.insert(index);
    }

    if (static_cast<int64_t>(instances.size()) == expectedNumberOfInstances)
    {
      return SeriesStatus_Complete;
    }
    else
    {
      return SeriesStatus_Missing;
    }
  }


  void StatelessDatabaseOperations::NormalizeLookup(std::vector<DatabaseConstraint>& target,
                                                    const DatabaseLookup& source,
                                                    ResourceType queryLevel) const
  {
    assert(mainDicomTagsRegistry_.get() != NULL);

    target.clear();
    target.reserve(source.GetConstraintsCount());

    for (size_t i = 0; i < source.GetConstraintsCount(); i++)
    {
      ResourceType level;
      DicomTagType type;
      
      mainDicomTagsRegistry_->LookupTag(level, type, source.GetConstraint(i).GetTag());

      if (type == DicomTagType_Identifier ||
          type == DicomTagType_Main)
      {
        // Use the fact that patient-level tags are copied at the study level
        if (level == ResourceType_Patient &&
            queryLevel != ResourceType_Patient)
        {
          level = ResourceType_Study;
        }
        
        target.push_back(source.GetConstraint(i).ConvertToDatabaseConstraint(level, type));
      }
    }
  }


  class StatelessDatabaseOperations::Transaction : public boost::noncopyable
  {
  private:
    IDatabaseWrapper&                                db_;
    std::unique_ptr<IDatabaseWrapper::ITransaction>  transaction_;
    std::unique_ptr<ITransactionContext>             context_;
    bool                                             isCommitted_;
    
  public:
    Transaction(IDatabaseWrapper& db,
                ITransactionContextFactory& factory,
                TransactionType type) :
      db_(db),
      isCommitted_(false)
    {
      context_.reset(factory.Create());
      if (context_.get() == NULL)
      {
        throw OrthancException(ErrorCode_NullPointer);
      }      
      
      transaction_.reset(db_.StartTransaction(type, *context_));
      if (transaction_.get() == NULL)
      {
        throw OrthancException(ErrorCode_NullPointer);
      }
    }

    ~Transaction()
    {
      if (!isCommitted_)
      {
        try
        {
          transaction_->Rollback();
        }
        catch (OrthancException& e)
        {
          LOG(INFO) << "Cannot rollback transaction: " << e.What();
        }
      }
    }

    IDatabaseWrapper::ITransaction& GetDatabaseTransaction()
    {
      assert(transaction_.get() != NULL);
      return *transaction_;
    }

    void Commit()
    {
      if (isCommitted_)
      {
        throw OrthancException(ErrorCode_BadSequenceOfCalls);
      }
      else
      {
        int64_t delta = context_->GetCompressedSizeDelta();

        transaction_->Commit(delta);
        context_->Commit();
        isCommitted_ = true;
      }
    }

    ITransactionContext& GetContext() const
    {
      assert(context_.get() != NULL);
      return *context_;
    }
  };
  

  void StatelessDatabaseOperations::ApplyInternal(IReadOnlyOperations* readOperations,
                                                  IReadWriteOperations* writeOperations)
  {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);  // To protect "factory_" and "maxRetries_"

    if ((readOperations == NULL && writeOperations == NULL) ||
        (readOperations != NULL && writeOperations != NULL))
    {
      throw OrthancException(ErrorCode_InternalError);
    }

    if (factory_.get() == NULL)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls, "No transaction context was provided");     
    }
    
    unsigned int attempt = 0;

    for (;;)
    {
      try
      {
        if (readOperations != NULL)
        {
          /**
           * IMPORTANT: In Orthanc <= 1.9.1, there was no transaction
           * in this case. This was OK because of the presence of the
           * global mutex that was protecting the database.
           **/
          
          Transaction transaction(db_, *factory_, TransactionType_ReadOnly);  // TODO - Only if not "TransactionType_Implicit"
          {
            ReadOnlyTransaction t(transaction.GetDatabaseTransaction(), transaction.GetContext());
            readOperations->Apply(t);
          }
          transaction.Commit();
        }
        else
        {
          assert(writeOperations != NULL);
          
          Transaction transaction(db_, *factory_, TransactionType_ReadWrite);
          {
            ReadWriteTransaction t(transaction.GetDatabaseTransaction(), transaction.GetContext());
            writeOperations->Apply(t);
          }
          transaction.Commit();
        }
        
        return;  // Success
      }
      catch (OrthancException& e)
      {
        if (e.GetErrorCode() == ErrorCode_DatabaseCannotSerialize)
        {
          if (attempt >= maxRetries_)
          {
            throw;
          }
          else
          {
            attempt++;

            // The "rand()" adds some jitter to de-synchronize writers
            boost::this_thread::sleep(boost::posix_time::milliseconds(100 * attempt + 5 * (rand() % 10)));
          }          
        }
        else
        {
          throw;
        }
      }
    }
  }

  
  StatelessDatabaseOperations::StatelessDatabaseOperations(IDatabaseWrapper& db) : 
    db_(db),
    mainDicomTagsRegistry_(new MainDicomTagsRegistry),
    hasFlushToDisk_(db.HasFlushToDisk()),
    maxRetries_(0)
  {
  }


  void StatelessDatabaseOperations::FlushToDisk()
  {
    try
    {
      db_.FlushToDisk();
    }
    catch (OrthancException&)
    {
      LOG(ERROR) << "Cannot flush the SQLite database to the disk (is your filesystem full?)";
    }
  }


  void StatelessDatabaseOperations::SetTransactionContextFactory(ITransactionContextFactory* factory)
  {
    boost::unique_lock<boost::shared_mutex> lock(mutex_);

    if (factory == NULL)
    {
      throw OrthancException(ErrorCode_NullPointer);
    }
    else if (factory_.get() != NULL)
    {
      throw OrthancException(ErrorCode_BadSequenceOfCalls);
    }
    else
    {
      factory_.reset(factory);
    }
  }
    

  void StatelessDatabaseOperations::SetMaxDatabaseRetries(unsigned int maxRetries)
  {
    boost::unique_lock<boost::shared_mutex> lock(mutex_);
    maxRetries_ = maxRetries;
  }
  

  void StatelessDatabaseOperations::Apply(IReadOnlyOperations& operations)
  {
    ApplyInternal(&operations, NULL);
  }
  

  void StatelessDatabaseOperations::Apply(IReadWriteOperations& operations)
  {
    ApplyInternal(NULL, &operations);
  }
  

  bool StatelessDatabaseOperations::ExpandResource(ExpandedResource& target,
                                                   const std::string& publicId,
                                                   ResourceType level,
                                                   const std::set<DicomTag>& requestedTags,
                                                   ExpandResourceDbFlags expandFlags)
  {    
    class Operations : public ReadOnlyOperationsT6<
      bool&, ExpandedResource&, const std::string&, ResourceType, const std::set<DicomTag>&, ExpandResourceDbFlags>
    {
    private:
  
      static bool LookupStringMetadata(std::string& result,
                                       const std::map<MetadataType, std::string>& metadata,
                                       MetadataType type)
      {
        std::map<MetadataType, std::string>::const_iterator found = metadata.find(type);

        if (found == metadata.end())
        {
          return false;
        }
        else
        {
          result = found->second;
          return true;
        }
      }


      static bool LookupIntegerMetadata(int64_t& result,
                                        const std::map<MetadataType, std::string>& metadata,
                                        MetadataType type)
      {
        std::string s;
        if (!LookupStringMetadata(s, metadata, type))
        {
          return false;
        }

        try
        {
          result = boost::lexical_cast<int64_t>(s);
          return true;
        }
        catch (boost::bad_lexical_cast&)
        {
          return false;
        }
      }


    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        // Lookup for the requested resource
        int64_t internalId;
        ResourceType type;
        std::string parent;
        if (!transaction.LookupResourceAndParent(internalId, type, parent, tuple.get<2>()) ||
            type != tuple.get<3>())
        {
          tuple.get<0>() = false;
        }
        else
        {
          ExpandedResource& target = tuple.get<1>();
          ExpandResourceDbFlags expandFlags = tuple.get<5>();

          // Set information about the parent resource (if it exists)
          if (type == ResourceType_Patient)
          {
            if (!parent.empty())
            {
              throw OrthancException(ErrorCode_DatabasePlugin);
            }
          }
          else
          {
            if (parent.empty())
            {
              throw OrthancException(ErrorCode_DatabasePlugin);
            }

            target.parentId_ = parent;
          }

          target.type_ = type;
          target.id_ = tuple.get<2>();

          if (expandFlags & ExpandResourceDbFlags_IncludeChildren)
          {
            // List the children resources
            transaction.GetChildrenPublicId(target.childrenIds_, internalId);
          }

          if (expandFlags & ExpandResourceDbFlags_IncludeMetadata)
          {
            // Extract the metadata
            transaction.GetAllMetadata(target.metadata_, internalId);

            switch (type)
            {
              case ResourceType_Patient:
              case ResourceType_Study:
                break;

              case ResourceType_Series:
              {
                int64_t i;
                if (LookupIntegerMetadata(i, target.metadata_, MetadataType_Series_ExpectedNumberOfInstances))
                {
                  target.expectedNumberOfInstances_ = static_cast<int>(i);
                  target.status_ = EnumerationToString(transaction.GetSeriesStatus(internalId, i));
                }
                else
                {
                  target.expectedNumberOfInstances_ = -1;
                  target.status_ = EnumerationToString(SeriesStatus_Unknown);
                }

                break;
              }

              case ResourceType_Instance:
              {
                FileInfo attachment;
                int64_t revision;  // ignored
                if (!transaction.LookupAttachment(attachment, revision, internalId, FileContentType_Dicom))
                {
                  throw OrthancException(ErrorCode_InternalError);
                }

                target.fileSize_ = static_cast<unsigned int>(attachment.GetUncompressedSize());
                target.fileUuid_ = attachment.GetUuid();

                int64_t i;
                if (LookupIntegerMetadata(i, target.metadata_, MetadataType_Instance_IndexInSeries))
                {
                  target.indexInSeries_ = static_cast<int>(i);
                }
                else
                {
                  target.indexInSeries_ = -1;
                }

                break;
              }

              default:
                throw OrthancException(ErrorCode_InternalError);
            }

            // check the main dicom tags list has not changed since the resource was stored
            target.mainDicomTagsSignature_ = DicomMap::GetDefaultMainDicomTagsSignature(type);
            LookupStringMetadata(target.mainDicomTagsSignature_, target.metadata_, MetadataType_MainDicomTagsSignature);
          }

          if (expandFlags & ExpandResourceDbFlags_IncludeMainDicomTags)
          {
            // read all tags from DB
            transaction.GetMainDicomTags(target.tags_, internalId);

            // read all main sequences from DB
            std::string serializedSequences;
            if (LookupStringMetadata(serializedSequences, target.metadata_, MetadataType_MainDicomSequences))
            {
              Json::Value jsonMetadata;
              Toolbox::ReadJson(jsonMetadata, serializedSequences);

              assert(jsonMetadata["Version"].asInt() == 1);
              target.sequences_.FromJson(jsonMetadata["Sequences"]);
            }

            // check if we have access to all requestedTags or if we must get tags from parents
            const std::set<DicomTag>& requestedTags = tuple.get<4>();

            if (requestedTags.size() > 0)
            {
              std::set<DicomTag> savedMainDicomTags;
              
              FromDcmtkBridge::ParseListOfTags(savedMainDicomTags, target.mainDicomTagsSignature_);

              // read parent main dicom tags as long as we have not gathered all requested tags
              ResourceType currentLevel = target.type_;
              int64_t currentInternalId = internalId;
              Toolbox::GetMissingsFromSet(target.missingRequestedTags_, requestedTags, savedMainDicomTags);

              while ((target.missingRequestedTags_.size() > 0)
                    && currentLevel != ResourceType_Patient)
              {
                currentLevel = GetParentResourceType(currentLevel);

                int64_t currentParentId;
                if (!transaction.LookupParent(currentParentId, currentInternalId))
                {
                  break;
                }

                std::map<MetadataType, std::string> parentMetadata;
                transaction.GetAllMetadata(parentMetadata, currentParentId);

                std::string parentMainDicomTagsSignature = DicomMap::GetDefaultMainDicomTagsSignature(currentLevel);
                LookupStringMetadata(parentMainDicomTagsSignature, parentMetadata, MetadataType_MainDicomTagsSignature);

                std::set<DicomTag> parentSavedMainDicomTags;
                FromDcmtkBridge::ParseListOfTags(parentSavedMainDicomTags, parentMainDicomTagsSignature);
                
                size_t previousMissingCount = target.missingRequestedTags_.size();
                Toolbox::AppendSets(savedMainDicomTags, parentSavedMainDicomTags);
                Toolbox::GetMissingsFromSet(target.missingRequestedTags_, requestedTags, savedMainDicomTags);

                // read the parent tags from DB only if it reduces the number of missing tags
                if (target.missingRequestedTags_.size() < previousMissingCount)
                { 
                  Toolbox::AppendSets(savedMainDicomTags, parentSavedMainDicomTags);

                  DicomMap parentTags;
                  transaction.GetMainDicomTags(parentTags, currentParentId);

                  target.tags_.Merge(parentTags);
                }

                currentInternalId = currentParentId;
              }
            }
          }

          std::string tmp;

          if (LookupStringMetadata(tmp, target.metadata_, MetadataType_AnonymizedFrom))
          {
            target.anonymizedFrom_ = tmp;
          }

          if (LookupStringMetadata(tmp, target.metadata_, MetadataType_ModifiedFrom))
          {
            target.modifiedFrom_ = tmp;
          }

          if (type == ResourceType_Patient ||
              type == ResourceType_Study ||
              type == ResourceType_Series)
          {
            target.isStable_ = !transaction.GetTransactionContext().IsUnstableResource(internalId);

            if (LookupStringMetadata(tmp, target.metadata_, MetadataType_LastUpdate))
            {
              target.lastUpdate_ = tmp;
            }
          }
          else
          {
            target.isStable_ = false;
          }

          tuple.get<0>() = true;
        }
      }
    };

    bool found;
    Operations operations;
    operations.Apply(*this, found, target, publicId, level, requestedTags, expandFlags);
    return found;
  }


  void StatelessDatabaseOperations::GetAllMetadata(std::map<MetadataType, std::string>& target,
                                                   const std::string& publicId,
                                                   ResourceType level)
  {
    class Operations : public ReadOnlyOperationsT3<std::map<MetadataType, std::string>&, const std::string&, ResourceType>
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        ResourceType type;
        int64_t id;
        if (!transaction.LookupResource(id, type, tuple.get<1>()) ||
            tuple.get<2>() != type)
        {
          throw OrthancException(ErrorCode_UnknownResource);
        }
        else
        {
          transaction.GetAllMetadata(tuple.get<0>(), id);
        }
      }
    };

    Operations operations;
    operations.Apply(*this, target, publicId, level);
  }


  bool StatelessDatabaseOperations::LookupAttachment(FileInfo& attachment,
                                                     int64_t& revision,
                                                     const std::string& instancePublicId,
                                                     FileContentType contentType)
  {
    class Operations : public ReadOnlyOperationsT5<bool&, FileInfo&, int64_t&, const std::string&, FileContentType>
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        int64_t internalId;
        ResourceType type;
        if (!transaction.LookupResource(internalId, type, tuple.get<3>()))
        {
          throw OrthancException(ErrorCode_UnknownResource);
        }
        else if (transaction.LookupAttachment(tuple.get<1>(), tuple.get<2>(), internalId, tuple.get<4>()))
        {
          assert(tuple.get<1>().GetContentType() == tuple.get<4>());
          tuple.get<0>() = true;
        }
        else
        {
          tuple.get<0>() = false;
        }
      }
    };

    bool found;
    Operations operations;
    operations.Apply(*this, found, attachment, revision, instancePublicId, contentType);
    return found;
  }


  void StatelessDatabaseOperations::GetAllUuids(std::list<std::string>& target,
                                                ResourceType resourceType)
  {
    class Operations : public ReadOnlyOperationsT2<std::list<std::string>&, ResourceType>
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        // TODO - CANDIDATE FOR "TransactionType_Implicit"
        transaction.GetAllPublicIds(tuple.get<0>(), tuple.get<1>());
      }
    };

    Operations operations;
    operations.Apply(*this, target, resourceType);
  }


  void StatelessDatabaseOperations::GetAllUuids(std::list<std::string>& target,
                                                ResourceType resourceType,
                                                size_t since,
                                                size_t limit)
  {
    if (limit == 0)
    {
      target.clear();
    }
    else
    {
      class Operations : public ReadOnlyOperationsT4<std::list<std::string>&, ResourceType, size_t, size_t>
      {
      public:
        virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                                const Tuple& tuple) ORTHANC_OVERRIDE
        {
          // TODO - CANDIDATE FOR "TransactionType_Implicit"
          transaction.GetAllPublicIds(tuple.get<0>(), tuple.get<1>(), tuple.get<2>(), tuple.get<3>());
        }
      };

      Operations operations;
      operations.Apply(*this, target, resourceType, since, limit);
    }
  }


  void StatelessDatabaseOperations::GetGlobalStatistics(/* out */ uint64_t& diskSize,
                                                        /* out */ uint64_t& uncompressedSize,
                                                        /* out */ uint64_t& countPatients, 
                                                        /* out */ uint64_t& countStudies, 
                                                        /* out */ uint64_t& countSeries, 
                                                        /* out */ uint64_t& countInstances)
  {
    class Operations : public ReadOnlyOperationsT6<uint64_t&, uint64_t&, uint64_t&, uint64_t&, uint64_t&, uint64_t&>
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        tuple.get<0>() = transaction.GetTotalCompressedSize();
        tuple.get<1>() = transaction.GetTotalUncompressedSize();
        tuple.get<2>() = transaction.GetResourcesCount(ResourceType_Patient);
        tuple.get<3>() = transaction.GetResourcesCount(ResourceType_Study);
        tuple.get<4>() = transaction.GetResourcesCount(ResourceType_Series);
        tuple.get<5>() = transaction.GetResourcesCount(ResourceType_Instance);
      }
    };
    
    Operations operations;
    operations.Apply(*this, diskSize, uncompressedSize, countPatients,
                     countStudies, countSeries, countInstances);
  }


  void StatelessDatabaseOperations::GetChanges(Json::Value& target,
                                               int64_t since,                               
                                               unsigned int maxResults)
  {
    class Operations : public ReadOnlyOperationsT3<Json::Value&, int64_t, unsigned int>
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        // NB: In Orthanc <= 1.3.2, a transaction was missing, as
        // "GetLastChange()" involves calls to "GetPublicId()"

        std::list<ServerIndexChange> changes;
        bool done;
        bool hasLast = false;
        int64_t last = 0;

        transaction.GetChanges(changes, done, tuple.get<1>(), tuple.get<2>());
        if (changes.empty())
        {
          last = transaction.GetLastChangeIndex();
          hasLast = true;
        }

        FormatLog(tuple.get<0>(), changes, "Changes", done, tuple.get<1>(), hasLast, last);
      }
    };
    
    Operations operations;
    operations.Apply(*this, target, since, maxResults);
  }


  void StatelessDatabaseOperations::GetLastChange(Json::Value& target)
  {
    class Operations : public ReadOnlyOperationsT1<Json::Value&>
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        // NB: In Orthanc <= 1.3.2, a transaction was missing, as
        // "GetLastChange()" involves calls to "GetPublicId()"

        std::list<ServerIndexChange> changes;
        bool hasLast = false;
        int64_t last = 0;

        transaction.GetLastChange(changes);
        if (changes.empty())
        {
          last = transaction.GetLastChangeIndex();
          hasLast = true;
        }

        FormatLog(tuple.get<0>(), changes, "Changes", true, 0, hasLast, last);
      }
    };
    
    Operations operations;
    operations.Apply(*this, target);
  }


  void StatelessDatabaseOperations::GetExportedResources(Json::Value& target,
                                                         int64_t since,
                                                         unsigned int maxResults)
  {
    class Operations : public ReadOnlyOperationsT3<Json::Value&, int64_t, unsigned int>
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        // TODO - CANDIDATE FOR "TransactionType_Implicit"

        std::list<ExportedResource> exported;
        bool done;
        transaction.GetExportedResources(exported, done, tuple.get<1>(), tuple.get<2>());
        FormatLog(tuple.get<0>(), exported, "Exports", done, tuple.get<1>(), false, -1);
      }
    };
    
    Operations operations;
    operations.Apply(*this, target, since, maxResults);
  }


  void StatelessDatabaseOperations::GetLastExportedResource(Json::Value& target)
  {
    class Operations : public ReadOnlyOperationsT1<Json::Value&>
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        // TODO - CANDIDATE FOR "TransactionType_Implicit"

        std::list<ExportedResource> exported;
        transaction.GetLastExportedResource(exported);
        FormatLog(tuple.get<0>(), exported, "Exports", true, 0, false, -1);
      }
    };
    
    Operations operations;
    operations.Apply(*this, target);
  }


  bool StatelessDatabaseOperations::IsProtectedPatient(const std::string& publicId)
  {
    class Operations : public ReadOnlyOperationsT2<bool&, const std::string&>
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        // Lookup for the requested resource
        int64_t id;
        ResourceType type;
        if (!transaction.LookupResource(id, type, tuple.get<1>()) ||
            type != ResourceType_Patient)
        {
          throw OrthancException(ErrorCode_ParameterOutOfRange);
        }
        else
        {
          tuple.get<0>() = transaction.IsProtectedPatient(id);
        }
      }
    };

    bool isProtected;
    Operations operations;
    operations.Apply(*this, isProtected, publicId);
    return isProtected;
  }


  void StatelessDatabaseOperations::GetChildren(std::list<std::string>& result,
                                                const std::string& publicId)
  {
    class Operations : public ReadOnlyOperationsT2<std::list<std::string>&, const std::string&>
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        ResourceType type;
        int64_t resource;
        if (!transaction.LookupResource(resource, type, tuple.get<1>()))
        {
          throw OrthancException(ErrorCode_UnknownResource);
        }
        else if (type == ResourceType_Instance)
        {
          // An instance cannot have a child
          throw OrthancException(ErrorCode_BadParameterType);
        }
        else
        {
          std::list<int64_t> tmp;
          transaction.GetChildrenInternalId(tmp, resource);

          tuple.get<0>().clear();

          for (std::list<int64_t>::const_iterator 
                 it = tmp.begin(); it != tmp.end(); ++it)
          {
            tuple.get<0>().push_back(transaction.GetPublicId(*it));
          }
        }
      }
    };
    
    Operations operations;
    operations.Apply(*this, result, publicId);
  }


  void StatelessDatabaseOperations::GetChildInstances(std::list<std::string>& result,
                                                      const std::string& publicId)
  {
    class Operations : public ReadOnlyOperationsT2<std::list<std::string>&, const std::string&>
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        tuple.get<0>().clear();
        
        ResourceType type;
        int64_t top;
        if (!transaction.LookupResource(top, type, tuple.get<1>()))
        {
          throw OrthancException(ErrorCode_UnknownResource);
        }
        else if (type == ResourceType_Instance)
        {
          // The resource is already an instance: Do not go down the hierarchy
          tuple.get<0>().push_back(tuple.get<1>());
        }
        else
        {
          std::stack<int64_t> toExplore;
          toExplore.push(top);

          std::list<int64_t> tmp;
          while (!toExplore.empty())
          {
            // Get the internal ID of the current resource
            int64_t resource = toExplore.top();
            toExplore.pop();

            // TODO - This could be optimized by seeing how many
            // levels "type == transaction.GetResourceType(top)" is
            // above the "instances level"
            if (transaction.GetResourceType(resource) == ResourceType_Instance)
            {
              tuple.get<0>().push_back(transaction.GetPublicId(resource));
            }
            else
            {
              // Tag all the children of this resource as to be explored
              transaction.GetChildrenInternalId(tmp, resource);
              for (std::list<int64_t>::const_iterator 
                     it = tmp.begin(); it != tmp.end(); ++it)
              {
                toExplore.push(*it);
              }
            }
          }
        }
      }
    };
    
    Operations operations;
    operations.Apply(*this, result, publicId);
  }


  bool StatelessDatabaseOperations::LookupMetadata(std::string& target,
                                                   int64_t& revision,
                                                   const std::string& publicId,
                                                   ResourceType expectedType,
                                                   MetadataType type)
  {
    class Operations : public ReadOnlyOperationsT6<bool&, std::string&, int64_t&,
                                                   const std::string&, ResourceType, MetadataType>
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        ResourceType resourceType;
        int64_t id;
        if (!transaction.LookupResource(id, resourceType, tuple.get<3>()) ||
            resourceType != tuple.get<4>())
        {
          throw OrthancException(ErrorCode_UnknownResource);
        }
        else
        {
          tuple.get<0>() = transaction.LookupMetadata(tuple.get<1>(), tuple.get<2>(), id, tuple.get<5>());
        }
      }
    };

    bool found;
    Operations operations;
    operations.Apply(*this, found, target, revision, publicId, expectedType, type);
    return found;
  }


  void StatelessDatabaseOperations::ListAvailableAttachments(std::set<FileContentType>& target,
                                                             const std::string& publicId,
                                                             ResourceType expectedType)
  {
    class Operations : public ReadOnlyOperationsT3<std::set<FileContentType>&, const std::string&, ResourceType>
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        ResourceType type;
        int64_t id;
        if (!transaction.LookupResource(id, type, tuple.get<1>()) ||
            tuple.get<2>() != type)
        {
          throw OrthancException(ErrorCode_UnknownResource);
        }
        else
        {
          transaction.ListAvailableAttachments(tuple.get<0>(), id);
        }
      }
    };
    
    Operations operations;
    operations.Apply(*this, target, publicId, expectedType);
  }


  bool StatelessDatabaseOperations::LookupParent(std::string& target,
                                                 const std::string& publicId)
  {
    class Operations : public ReadOnlyOperationsT3<bool&, std::string&, const std::string&>
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        ResourceType type;
        int64_t id;
        if (!transaction.LookupResource(id, type, tuple.get<2>()))
        {
          throw OrthancException(ErrorCode_UnknownResource);
        }
        else
        {
          int64_t parentId;
          if (transaction.LookupParent(parentId, id))
          {
            tuple.get<1>() = transaction.GetPublicId(parentId);
            tuple.get<0>() = true;
          }
          else
          {
            tuple.get<0>() = false;
          }
        }
      }
    };

    bool found;
    Operations operations;
    operations.Apply(*this, found, target, publicId);
    return found;
  }


  void StatelessDatabaseOperations::GetResourceStatistics(/* out */ ResourceType& type,
                                                          /* out */ uint64_t& diskSize, 
                                                          /* out */ uint64_t& uncompressedSize, 
                                                          /* out */ unsigned int& countStudies, 
                                                          /* out */ unsigned int& countSeries, 
                                                          /* out */ unsigned int& countInstances, 
                                                          /* out */ uint64_t& dicomDiskSize, 
                                                          /* out */ uint64_t& dicomUncompressedSize, 
                                                          const std::string& publicId)
  {
    class Operations : public IReadOnlyOperations
    {
    private:
      ResourceType&      type_;
      uint64_t&          diskSize_; 
      uint64_t&          uncompressedSize_; 
      unsigned int&      countStudies_; 
      unsigned int&      countSeries_; 
      unsigned int&      countInstances_; 
      uint64_t&          dicomDiskSize_; 
      uint64_t&          dicomUncompressedSize_; 
      const std::string& publicId_;
        
    public:
      explicit Operations(ResourceType& type,
                          uint64_t& diskSize, 
                          uint64_t& uncompressedSize, 
                          unsigned int& countStudies, 
                          unsigned int& countSeries, 
                          unsigned int& countInstances, 
                          uint64_t& dicomDiskSize, 
                          uint64_t& dicomUncompressedSize, 
                          const std::string& publicId) :
        type_(type),
        diskSize_(diskSize),
        uncompressedSize_(uncompressedSize),
        countStudies_(countStudies),
        countSeries_(countSeries),
        countInstances_(countInstances),
        dicomDiskSize_(dicomDiskSize),
        dicomUncompressedSize_(dicomUncompressedSize),
        publicId_(publicId)
      {
      }
      
      virtual void Apply(ReadOnlyTransaction& transaction) ORTHANC_OVERRIDE
      {
        int64_t top;
        if (!transaction.LookupResource(top, type_, publicId_))
        {
          throw OrthancException(ErrorCode_UnknownResource);
        }
        else
        {
          countInstances_ = 0;
          countSeries_ = 0;
          countStudies_ = 0;
          diskSize_ = 0;
          uncompressedSize_ = 0;
          dicomDiskSize_ = 0;
          dicomUncompressedSize_ = 0;

          std::stack<int64_t> toExplore;
          toExplore.push(top);

          while (!toExplore.empty())
          {
            // Get the internal ID of the current resource
            int64_t resource = toExplore.top();
            toExplore.pop();

            ResourceType thisType = transaction.GetResourceType(resource);

            std::set<FileContentType> f;
            transaction.ListAvailableAttachments(f, resource);

            for (std::set<FileContentType>::const_iterator
                   it = f.begin(); it != f.end(); ++it)
            {
              FileInfo attachment;
              int64_t revision;  // ignored
              if (transaction.LookupAttachment(attachment, revision, resource, *it))
              {
                if (attachment.GetContentType() == FileContentType_Dicom)
                {
                  dicomDiskSize_ += attachment.GetCompressedSize();
                  dicomUncompressedSize_ += attachment.GetUncompressedSize();
                }
          
                diskSize_ += attachment.GetCompressedSize();
                uncompressedSize_ += attachment.GetUncompressedSize();
              }
            }

            if (thisType == ResourceType_Instance)
            {
              countInstances_++;
            }
            else
            {
              switch (thisType)
              {
                case ResourceType_Study:
                  countStudies_++;
                  break;

                case ResourceType_Series:
                  countSeries_++;
                  break;

                default:
                  break;
              }

              // Tag all the children of this resource as to be explored
              std::list<int64_t> tmp;
              transaction.GetChildrenInternalId(tmp, resource);
              for (std::list<int64_t>::const_iterator 
                     it = tmp.begin(); it != tmp.end(); ++it)
              {
                toExplore.push(*it);
              }
            }
          }

          if (countStudies_ == 0)
          {
            countStudies_ = 1;
          }

          if (countSeries_ == 0)
          {
            countSeries_ = 1;
          }
        }
      }
    };

    Operations operations(type, diskSize, uncompressedSize, countStudies, countSeries,
                          countInstances, dicomDiskSize, dicomUncompressedSize, publicId);
    Apply(operations);
  }


  void StatelessDatabaseOperations::LookupIdentifierExact(std::vector<std::string>& result,
                                                          ResourceType level,
                                                          const DicomTag& tag,
                                                          const std::string& value)
  {
    assert((level == ResourceType_Patient && tag == DICOM_TAG_PATIENT_ID) ||
           (level == ResourceType_Study && tag == DICOM_TAG_STUDY_INSTANCE_UID) ||
           (level == ResourceType_Study && tag == DICOM_TAG_ACCESSION_NUMBER) ||
           (level == ResourceType_Series && tag == DICOM_TAG_SERIES_INSTANCE_UID) ||
           (level == ResourceType_Instance && tag == DICOM_TAG_SOP_INSTANCE_UID));
    
    result.clear();

    DicomTagConstraint c(tag, ConstraintType_Equal, value, true, true);

    std::vector<DatabaseConstraint> query;
    query.push_back(c.ConvertToDatabaseConstraint(level, DicomTagType_Identifier));


    class Operations : public IReadOnlyOperations
    {
    private:
      std::vector<std::string>&               result_;
      const std::vector<DatabaseConstraint>&  query_;
      ResourceType                            level_;
      
    public:
      Operations(std::vector<std::string>& result,
                 const std::vector<DatabaseConstraint>& query,
                 ResourceType level) :
        result_(result),
        query_(query),
        level_(level)
      {
      }

      virtual void Apply(ReadOnlyTransaction& transaction) ORTHANC_OVERRIDE
      {
        // TODO - CANDIDATE FOR "TransactionType_Implicit"
        std::list<std::string> tmp;
        transaction.ApplyLookupResources(tmp, NULL, query_, level_, 0);
        CopyListToVector(result_, tmp);
      }
    };

    Operations operations(result, query, level);
    Apply(operations);
  }


  bool StatelessDatabaseOperations::LookupGlobalProperty(std::string& value,
                                                         GlobalProperty property,
                                                         bool shared)
  {
    class Operations : public ReadOnlyOperationsT4<bool&, std::string&, GlobalProperty, bool>
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        // TODO - CANDIDATE FOR "TransactionType_Implicit"
        tuple.get<0>() = transaction.LookupGlobalProperty(tuple.get<1>(), tuple.get<2>(), tuple.get<3>());
      }
    };

    bool found;
    Operations operations;
    operations.Apply(*this, found, value, property, shared);
    return found;
  }
  

  std::string StatelessDatabaseOperations::GetGlobalProperty(GlobalProperty property,
                                                             bool shared,
                                                             const std::string& defaultValue)
  {
    std::string s;
    if (LookupGlobalProperty(s, property, shared))
    {
      return s;
    }
    else
    {
      return defaultValue;
    }
  }


  bool StatelessDatabaseOperations::GetMainDicomTags(DicomMap& result,
                                                     const std::string& publicId,
                                                     ResourceType expectedType,
                                                     ResourceType levelOfInterest)
  {
    // Yes, the following test could be shortened, but we wish to make it as clear as possible
    if (!(expectedType == ResourceType_Patient  && levelOfInterest == ResourceType_Patient) &&
        !(expectedType == ResourceType_Study    && levelOfInterest == ResourceType_Patient) &&
        !(expectedType == ResourceType_Study    && levelOfInterest == ResourceType_Study)   &&
        !(expectedType == ResourceType_Series   && levelOfInterest == ResourceType_Series)  &&
        !(expectedType == ResourceType_Instance && levelOfInterest == ResourceType_Instance))
    {
      throw OrthancException(ErrorCode_ParameterOutOfRange);
    }


    class Operations : public ReadOnlyOperationsT5<bool&, DicomMap&, const std::string&, ResourceType, ResourceType>
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        // Lookup for the requested resource
        int64_t id;
        ResourceType type;
        if (!transaction.LookupResource(id, type, tuple.get<2>()) ||
            type != tuple.get<3>())
        {
          tuple.get<0>() = false;
        }
        else if (type == ResourceType_Study)
        {
          DicomMap tmp;
          transaction.GetMainDicomTags(tmp, id);

          switch (tuple.get<4>())
          {
            case ResourceType_Patient:
              tmp.ExtractPatientInformation(tuple.get<1>());
              tuple.get<0>() = true;
              break;

            case ResourceType_Study:
              tmp.ExtractStudyInformation(tuple.get<1>());
              tuple.get<0>() = true;
              break;

            default:
              throw OrthancException(ErrorCode_InternalError);
          }
        }
        else
        {
          transaction.GetMainDicomTags(tuple.get<1>(), id);
          tuple.get<0>() = true;
        }    
      }
    };

    result.Clear();

    bool found;
    Operations operations;
    operations.Apply(*this, found, result, publicId, expectedType, levelOfInterest);
    return found;
  }


  bool StatelessDatabaseOperations::GetAllMainDicomTags(DicomMap& result,
                                                        const std::string& instancePublicId)
  {
    class Operations : public ReadOnlyOperationsT3<bool&, DicomMap&, const std::string&>
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        // Lookup for the requested resource
        int64_t instance;
        ResourceType type;
        if (!transaction.LookupResource(instance, type, tuple.get<2>()) ||
            type != ResourceType_Instance)
        {
          tuple.get<0>() =  false;
        }
        else
        {
          DicomMap tmp;

          transaction.GetMainDicomTags(tmp, instance);
          tuple.get<1>().Merge(tmp);

          int64_t series;
          if (!transaction.LookupParent(series, instance))
          {
            throw OrthancException(ErrorCode_InternalError);
          }

          tmp.Clear();
          transaction.GetMainDicomTags(tmp, series);
          tuple.get<1>().Merge(tmp);

          int64_t study;
          if (!transaction.LookupParent(study, series))
          {
            throw OrthancException(ErrorCode_InternalError);
          }

          tmp.Clear();
          transaction.GetMainDicomTags(tmp, study);
          tuple.get<1>().Merge(tmp);

#ifndef NDEBUG
          {
            // Sanity test to check that all the main DICOM tags from the
            // patient level are copied at the study level
        
            int64_t patient;
            if (!transaction.LookupParent(patient, study))
            {
              throw OrthancException(ErrorCode_InternalError);
            }

            tmp.Clear();
            transaction.GetMainDicomTags(tmp, study);

            std::set<DicomTag> patientTags;
            tmp.GetTags(patientTags);

            for (std::set<DicomTag>::const_iterator
                   it = patientTags.begin(); it != patientTags.end(); ++it)
            {
              assert(tuple.get<1>().HasTag(*it));
            }
          }
#endif
      
          tuple.get<0>() =  true;
        }
      }
    };

    result.Clear();
    
    bool found;
    Operations operations;
    operations.Apply(*this, found, result, instancePublicId);
    return found;
  }


  bool StatelessDatabaseOperations::LookupResourceType(ResourceType& type,
                                                       const std::string& publicId)
  {
    class Operations : public ReadOnlyOperationsT3<bool&, ResourceType&, const std::string&>
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        // TODO - CANDIDATE FOR "TransactionType_Implicit"
        int64_t id;
        tuple.get<0>() = transaction.LookupResource(id, tuple.get<1>(), tuple.get<2>());
      }
    };

    bool found;
    Operations operations;
    operations.Apply(*this, found, type, publicId);
    return found;
  }


  bool StatelessDatabaseOperations::LookupParent(std::string& target,
                                                 const std::string& publicId,
                                                 ResourceType parentType)
  {
    class Operations : public ReadOnlyOperationsT4<bool&, std::string&, const std::string&, ResourceType>
    {
    public:
      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        ResourceType type;
        int64_t id;
        if (!transaction.LookupResource(id, type, tuple.get<2>()))
        {
          throw OrthancException(ErrorCode_UnknownResource);
        }

        while (type != tuple.get<3>())
        {
          int64_t parentId;

          if (type == ResourceType_Patient ||    // Cannot further go up in hierarchy
              !transaction.LookupParent(parentId, id))
          {
            tuple.get<0>() = false;
            return;
          }

          id = parentId;
          type = GetParentResourceType(type);
        }

        tuple.get<0>() = true;
        tuple.get<1>() = transaction.GetPublicId(id);
      }
    };

    bool found;
    Operations operations;
    operations.Apply(*this, found, target, publicId, parentType);
    return found;
  }


  void StatelessDatabaseOperations::ApplyLookupResources(std::vector<std::string>& resourcesId,
                                                         std::vector<std::string>* instancesId,
                                                         const DatabaseLookup& lookup,
                                                         ResourceType queryLevel,
                                                         size_t limit)
  {
    class Operations : public ReadOnlyOperationsT4<bool, const std::vector<DatabaseConstraint>&, ResourceType, size_t>
    {
    private:
      std::list<std::string>  resourcesList_;
      std::list<std::string>  instancesList_;
      
    public:
      const std::list<std::string>& GetResourcesList() const
      {
        return resourcesList_;
      }

      const std::list<std::string>& GetInstancesList() const
      {
        return instancesList_;
      }

      virtual void ApplyTuple(ReadOnlyTransaction& transaction,
                              const Tuple& tuple) ORTHANC_OVERRIDE
      {
        // TODO - CANDIDATE FOR "TransactionType_Implicit"
        if (tuple.get<0>())
        {
          transaction.ApplyLookupResources(resourcesList_, &instancesList_, tuple.get<1>(), tuple.get<2>(), tuple.get<3>());
        }
        else
        {
          transaction.ApplyLookupResources(resourcesList_, NULL, tuple.get<1>(), tuple.get<2>(), tuple.get<3>());
        }
      }
    };


    std::vector<DatabaseConstraint> normalized;
    NormalizeLookup(normalized, lookup, queryLevel);

    Operations operations;
    operations.Apply(*this, (instancesId != NULL), normalized, queryLevel, limit);
    
    CopyListToVector(resourcesId, operations.GetResourcesList());

    if (instancesId != NULL)
    { 
      CopyListToVector(*instancesId, operations.GetInstancesList());
    }
  }


  bool StatelessDatabaseOperations::DeleteResource(Json::Value& remainingAncestor,
                                                   const std::string& uuid,
                                                   ResourceType expectedType)
  {
    class Operations : public IReadWriteOperations
    {
    private:
      bool                found_;
      Json::Value&        remainingAncestor_;
      const std::string&  uuid_;
      ResourceType        expectedType_;
      
    public:
      Operations(Json::Value& remainingAncestor,
                 const std::string& uuid,
                 ResourceType expectedType) :
        found_(false),
        remainingAncestor_(remainingAncestor),
        uuid_(uuid),
        expectedType_(expectedType)
      {
      }

      bool IsFound() const
      {
        return found_;
      }

      virtual void Apply(ReadWriteTransaction& transaction) ORTHANC_OVERRIDE
      {
        int64_t id;
        ResourceType type;
        if (!transaction.LookupResource(id, type, uuid_) ||
            expectedType_ != type)
        {
          found_ = false;
        }
        else
        {
          found_ = true;
          transaction.DeleteResource(id);

          std::string remainingPublicId;
          ResourceType remainingLevel;
          if (transaction.GetTransactionContext().LookupRemainingLevel(remainingPublicId, remainingLevel))
          {
            remainingAncestor_["RemainingAncestor"] = Json::Value(Json::objectValue);
            remainingAncestor_["RemainingAncestor"]["Path"] = GetBasePath(remainingLevel, remainingPublicId);
            remainingAncestor_["RemainingAncestor"]["Type"] = EnumerationToString(remainingLevel);
            remainingAncestor_["RemainingAncestor"]["ID"] = remainingPublicId;
          }
          else
          {
            remainingAncestor_["RemainingAncestor"] = Json::nullValue;
          }
        }
      }
    };

    Operations operations(remainingAncestor, uuid, expectedType);
    Apply(operations);
    return operations.IsFound();
  }


  void StatelessDatabaseOperations::LogExportedResource(const std::string& publicId,
                                                        const std::string& remoteModality)
  {
    class Operations : public IReadWriteOperations
    {
    private:
      const std::string&  publicId_;
      const std::string&  remoteModality_;

    public:
      Operations(const std::string& publicId,
                 const std::string& remoteModality) :
        publicId_(publicId),
        remoteModality_(remoteModality)
      {
      }
      
      virtual void Apply(ReadWriteTransaction& transaction) ORTHANC_OVERRIDE
      {
        int64_t id;
        ResourceType type;
        if (!transaction.LookupResource(id, type, publicId_))
        {
          throw OrthancException(ErrorCode_InexistentItem);
        }

        std::string patientId;
        std::string studyInstanceUid;
        std::string seriesInstanceUid;
        std::string sopInstanceUid;

        int64_t currentId = id;
        ResourceType currentType = type;

        // Iteratively go up inside the patient/study/series/instance hierarchy
        bool done = false;
        while (!done)
        {
          DicomMap map;
          transaction.GetMainDicomTags(map, currentId);

          switch (currentType)
          {
            case ResourceType_Patient:
              if (map.HasTag(DICOM_TAG_PATIENT_ID))
              {
                patientId = map.GetValue(DICOM_TAG_PATIENT_ID).GetContent();
              }
              done = true;
              break;

            case ResourceType_Study:
              if (map.HasTag(DICOM_TAG_STUDY_INSTANCE_UID))
              {
                studyInstanceUid = map.GetValue(DICOM_TAG_STUDY_INSTANCE_UID).GetContent();
              }
              currentType = ResourceType_Patient;
              break;

            case ResourceType_Series:
              if (map.HasTag(DICOM_TAG_SERIES_INSTANCE_UID))
              {
                seriesInstanceUid = map.GetValue(DICOM_TAG_SERIES_INSTANCE_UID).GetContent();
              }
              currentType = ResourceType_Study;
              break;

            case ResourceType_Instance:
              if (map.HasTag(DICOM_TAG_SOP_INSTANCE_UID))
              {
                sopInstanceUid = map.GetValue(DICOM_TAG_SOP_INSTANCE_UID).GetContent();
              }
              currentType = ResourceType_Series;
              break;

            default:
              throw OrthancException(ErrorCode_InternalError);
          }

          // If we have not reached the Patient level, find the parent of
          // the current resource
          if (!done)
          {
            bool ok = transaction.LookupParent(currentId, currentId);
            (void) ok;  // Remove warning about unused variable in release builds
            assert(ok);
          }
        }

        ExportedResource resource(-1, 
                                  type,
                                  publicId_,
                                  remoteModality_,
                                  SystemToolbox::GetNowIsoString(true /* use UTC time (not local time) */),
                                  patientId,
                                  studyInstanceUid,
                                  seriesInstanceUid,
                                  sopInstanceUid);

        transaction.LogExportedResource(resource);
      }
    };

    Operations operations(publicId, remoteModality);
    Apply(operations);
  }


  void StatelessDatabaseOperations::SetProtectedPatient(const std::string& publicId,
                                                        bool isProtected)
  {
    class Operations : public IReadWriteOperations
    {
    private:
      const std::string&  publicId_;
      bool                isProtected_;

    public:
      Operations(const std::string& publicId,
                 bool isProtected) :
        publicId_(publicId),
        isProtected_(isProtected)
      {
      }

      virtual void Apply(ReadWriteTransaction& transaction) ORTHANC_OVERRIDE
      {
        // Lookup for the requested resource
        int64_t id;
        ResourceType type;
        if (!transaction.LookupResource(id, type, publicId_) ||
            type != ResourceType_Patient)
        {
          throw OrthancException(ErrorCode_ParameterOutOfRange);
        }
        else
        {
          transaction.SetProtectedPatient(id, isProtected_);
        }
      }
    };

    Operations operations(publicId, isProtected);
    Apply(operations);

    if (isProtected)
    {
      LOG(INFO) << "Patient " << publicId << " has been protected";
    }
    else
    {
      LOG(INFO) << "Patient " << publicId << " has been unprotected";
    }
  }


  void StatelessDatabaseOperations::SetMetadata(int64_t& newRevision,
                                                const std::string& publicId,
                                                MetadataType type,
                                                const std::string& value,
                                                bool hasOldRevision,
                                                int64_t oldRevision,
                                                const std::string& oldMD5)
  {
    class Operations : public IReadWriteOperations
    {
    private:
      int64_t&            newRevision_;
      const std::string&  publicId_;
      MetadataType        type_;
      const std::string&  value_;
      bool                hasOldRevision_;
      int64_t             oldRevision_;
      const std::string&  oldMD5_;

    public:
      Operations(int64_t& newRevision,
                 const std::string& publicId,
                 MetadataType type,
                 const std::string& value,
                 bool hasOldRevision,
                 int64_t oldRevision,
                 const std::string& oldMD5) :
        newRevision_(newRevision),
        publicId_(publicId),
        type_(type),
        value_(value),
        hasOldRevision_(hasOldRevision),
        oldRevision_(oldRevision),
        oldMD5_(oldMD5)
      {
      }

      virtual void Apply(ReadWriteTransaction& transaction) ORTHANC_OVERRIDE
      {
        ResourceType resourceType;
        int64_t id;
        if (!transaction.LookupResource(id, resourceType, publicId_))
        {
          throw OrthancException(ErrorCode_UnknownResource);
        }
        else
        {
          std::string oldValue;
          int64_t expectedRevision;
          if (transaction.LookupMetadata(oldValue, expectedRevision, id, type_))
          {
            if (hasOldRevision_)
            {
              std::string expectedMD5;
              Toolbox::ComputeMD5(expectedMD5, oldValue);

              if (expectedRevision != oldRevision_ ||
                  expectedMD5 != oldMD5_)
              {
                throw OrthancException(ErrorCode_Revision);
              }              
            }
            
            newRevision_ = expectedRevision + 1;
          }
          else
          {
            // The metadata is not existing yet: Ignore "oldRevision"
            // and initialize a new sequence of revisions
            newRevision_ = 0;
          }

          transaction.SetMetadata(id, type_, value_, newRevision_);
          
          if (IsUserMetadata(type_))
          {
            transaction.LogChange(id, ChangeType_UpdatedMetadata, resourceType, publicId_);
          }
        }
      }
    };

    Operations operations(newRevision, publicId, type, value, hasOldRevision, oldRevision, oldMD5);
    Apply(operations);
  }


  void StatelessDatabaseOperations::OverwriteMetadata(const std::string& publicId,
                                                      MetadataType type,
                                                      const std::string& value)
  {
    int64_t newRevision;  // Unused
    SetMetadata(newRevision, publicId, type, value, false /* no old revision */, -1 /* dummy */, "" /* dummy */);
  }


  bool StatelessDatabaseOperations::DeleteMetadata(const std::string& publicId,
                                                   MetadataType type,
                                                   bool hasRevision,
                                                   int64_t revision,
                                                   const std::string& md5)
  {
    class Operations : public IReadWriteOperations
    {
    private:
      const std::string&  publicId_;
      MetadataType        type_;
      bool                hasRevision_;
      int64_t             revision_;
      const std::string&  md5_;
      bool                found_;

    public:
      Operations(const std::string& publicId,
                 MetadataType type,
                 bool hasRevision,
                 int64_t revision,
                 const std::string& md5) :
        publicId_(publicId),
        type_(type),
        hasRevision_(hasRevision),
        revision_(revision),
        md5_(md5),
        found_(false)
      {
      }

      bool HasFound() const
      {
        return found_;
      }

      virtual void Apply(ReadWriteTransaction& transaction) ORTHANC_OVERRIDE
      {
        ResourceType resourceType;
        int64_t id;
        if (!transaction.LookupResource(id, resourceType, publicId_))
        {
          throw OrthancException(ErrorCode_UnknownResource);
        }
        else
        {
          std::string value;
          int64_t expectedRevision;
          if (transaction.LookupMetadata(value, expectedRevision, id, type_))
          {
            if (hasRevision_)
            {
              std::string expectedMD5;
              Toolbox::ComputeMD5(expectedMD5, value);

              if (expectedRevision != revision_ ||
                  expectedMD5 != md5_)
              {
                throw OrthancException(ErrorCode_Revision);
              }
            }
            
            found_ = true;
            transaction.DeleteMetadata(id, type_);
            
            if (IsUserMetadata(type_))
            {
              transaction.LogChange(id, ChangeType_UpdatedMetadata, resourceType, publicId_);
            }
          }
          else
          {
            found_ = false;
          }
        }
      }
    };

    Operations operations(publicId, type, hasRevision, revision, md5);
    Apply(operations);
    return operations.HasFound();
  }


  uint64_t StatelessDatabaseOperations::IncrementGlobalSequence(GlobalProperty sequence,
                                                                bool shared)
  {
    class Operations : public IReadWriteOperations
    {
    private:
      uint64_t       newValue_;
      GlobalProperty sequence_;
      bool           shared_;

    public:
      Operations(GlobalProperty sequence,
                 bool shared) :
        newValue_(0),  // Dummy initialization
        sequence_(sequence),
        shared_(shared)
      {
      }

      uint64_t GetNewValue() const
      {
        return newValue_;
      }

      virtual void Apply(ReadWriteTransaction& transaction) ORTHANC_OVERRIDE
      {
        std::string oldString;

        if (transaction.LookupGlobalProperty(oldString, sequence_, shared_))
        {
          uint64_t oldValue;
      
          try
          {
            oldValue = boost::lexical_cast<uint64_t>(oldString);
          }
          catch (boost::bad_lexical_cast&)
          {
            LOG(ERROR) << "Cannot read the global sequence "
                       << boost::lexical_cast<std::string>(sequence_) << ", resetting it";
            oldValue = 0;
          }

          newValue_ = oldValue + 1;
        }
        else
        {
          // Initialize the sequence at "1"
          newValue_ = 1;
        }

        transaction.SetGlobalProperty(sequence_, shared_, boost::lexical_cast<std::string>(newValue_));
      }
    };

    Operations operations(sequence, shared);
    Apply(operations);
    assert(operations.GetNewValue() != 0);
    return operations.GetNewValue();
  }


  void StatelessDatabaseOperations::DeleteChanges()
  {
    class Operations : public IReadWriteOperations
    {
    public:
      virtual void Apply(ReadWriteTransaction& transaction) ORTHANC_OVERRIDE
      {
        transaction.ClearChanges();
      }
    };

    Operations operations;
    Apply(operations);
  }

  
  void StatelessDatabaseOperations::DeleteExportedResources()
  {
    class Operations : public IReadWriteOperations
    {
    public:
      virtual void Apply(ReadWriteTransaction& transaction) ORTHANC_OVERRIDE
      {
        transaction.ClearExportedResources();
      }
    };

    Operations operations;
    Apply(operations);
  }


  void StatelessDatabaseOperations::SetGlobalProperty(GlobalProperty property,
                                                      bool shared,
                                                      const std::string& value)
  {
    class Operations : public IReadWriteOperations
    {
    private:
      GlobalProperty      property_;
      bool                shared_;
      const std::string&  value_;
      
    public:
      Operations(GlobalProperty property,
                 bool shared,
                 const std::string& value) :
        property_(property),
        shared_(shared),
        value_(value)
      {
      }
        
      virtual void Apply(ReadWriteTransaction& transaction) ORTHANC_OVERRIDE
      {
        transaction.SetGlobalProperty(property_, shared_, value_);
      }
    };

    Operations operations(property, shared, value);
    Apply(operations);
  }


  bool StatelessDatabaseOperations::DeleteAttachment(const std::string& publicId,
                                                     FileContentType type,
                                                     bool hasRevision,
                                                     int64_t revision,
                                                     const std::string& md5)
  {
    class Operations : public IReadWriteOperations
    {
    private:
      const std::string&  publicId_;
      FileContentType     type_;
      bool                hasRevision_;
      int64_t             revision_;
      const std::string&  md5_;
      bool                found_;

    public:
      Operations(const std::string& publicId,
                 FileContentType type,
                 bool hasRevision,
                 int64_t revision,
                 const std::string& md5) :
        publicId_(publicId),
        type_(type),
        hasRevision_(hasRevision),
        revision_(revision),
        md5_(md5),
        found_(false)
      {
      }
        
      bool HasFound() const
      {
        return found_;
      }
      
      virtual void Apply(ReadWriteTransaction& transaction) ORTHANC_OVERRIDE
      {
        ResourceType resourceType;
        int64_t id;
        if (!transaction.LookupResource(id, resourceType, publicId_))
        {
          throw OrthancException(ErrorCode_UnknownResource);
        }
        else
        {
          FileInfo info;
          int64_t expectedRevision;
          if (transaction.LookupAttachment(info, expectedRevision, id, type_))
          {
            if (hasRevision_ &&
                (expectedRevision != revision_ ||
                 info.GetUncompressedMD5() != md5_))
            {
              throw OrthancException(ErrorCode_Revision);
            }
            
            found_ = true;
            transaction.DeleteAttachment(id, type_);
          
            if (IsUserContentType(type_))
            {
              transaction.LogChange(id, ChangeType_UpdatedAttachment, resourceType, publicId_);
            }
          }
          else
          {
            found_ = false;
          }
        }
      }
    };

    Operations operations(publicId, type, hasRevision, revision, md5);
    Apply(operations);
    return operations.HasFound();
  }


  void StatelessDatabaseOperations::LogChange(int64_t internalId,
                                              ChangeType changeType,
                                              const std::string& publicId,
                                              ResourceType level)
  {
    class Operations : public IReadWriteOperations
    {
    private:
      int64_t             internalId_;
      ChangeType          changeType_;
      const std::string&  publicId_;
      ResourceType        level_;
      
    public:
      Operations(int64_t internalId,
                 ChangeType changeType,
                 const std::string& publicId,
                 ResourceType level) :
        internalId_(internalId),
        changeType_(changeType),
        publicId_(publicId),
        level_(level)
      {
      }
        
      virtual void Apply(ReadWriteTransaction& transaction) ORTHANC_OVERRIDE
      {
        int64_t id;
        ResourceType type;
        if (transaction.LookupResource(id, type, publicId_) &&
            id == internalId_)
        {
          /**
           * Make sure that the resource is still existing, with the
           * same internal ID, which indicates the absence of bouncing
           * (if deleting then recreating the same resource). Don't
           * throw an exception if the resource has been deleted,
           * because this function might e.g. be called from
           * "StatelessDatabaseOperations::UnstableResourcesMonitorThread()"
           * (for which a deleted resource is *not* an error case).
           **/
          if (type == level_)
          {
            transaction.LogChange(id, changeType_, type, publicId_);
          }
          else
          {
            // Consistency check
            throw OrthancException(ErrorCode_UnknownResource);
          }
        }
      }
    };

    Operations operations(internalId, changeType, publicId, level);
    Apply(operations);
  }


  void StatelessDatabaseOperations::ReconstructInstance(const ParsedDicomFile& dicom)
  {
    class Operations : public IReadWriteOperations
    {
    private:
      DicomMap                              summary_;
      std::unique_ptr<DicomInstanceHasher>  hasher_;
      bool                                  hasTransferSyntax_;
      DicomTransferSyntax                   transferSyntax_;

      static void ReplaceMetadata(ReadWriteTransaction& transaction,
                                  int64_t instance,
                                  MetadataType metadata,
                                  const std::string& value)
      {
        std::string oldValue;
        int64_t oldRevision;
        
        if (transaction.LookupMetadata(oldValue, oldRevision, instance, metadata))
        {
          transaction.SetMetadata(instance, metadata, value, oldRevision + 1);
        }
        else
        {
          transaction.SetMetadata(instance, metadata, value, 0);
        }
      }
      
    public:
      explicit Operations(const ParsedDicomFile& dicom)
      {
        OrthancConfiguration::DefaultExtractDicomSummary(summary_, dicom);
        hasher_.reset(new DicomInstanceHasher(summary_));
        hasTransferSyntax_ = dicom.LookupTransferSyntax(transferSyntax_);
      }
        
      virtual void Apply(ReadWriteTransaction& transaction) ORTHANC_OVERRIDE
      {
        int64_t patient = -1, study = -1, series = -1, instance = -1;

        ResourceType type1, type2, type3, type4;      
        if (!transaction.LookupResource(patient, type1, hasher_->HashPatient()) ||
            !transaction.LookupResource(study, type2, hasher_->HashStudy()) ||
            !transaction.LookupResource(series, type3, hasher_->HashSeries()) ||
            !transaction.LookupResource(instance, type4, hasher_->HashInstance()) ||
            type1 != ResourceType_Patient ||
            type2 != ResourceType_Study ||
            type3 != ResourceType_Series ||
            type4 != ResourceType_Instance ||
            patient == -1 ||
            study == -1 ||
            series == -1 ||
            instance == -1)
        {
          throw OrthancException(ErrorCode_InternalError);
        }

        transaction.ClearMainDicomTags(patient);
        transaction.ClearMainDicomTags(study);
        transaction.ClearMainDicomTags(series);
        transaction.ClearMainDicomTags(instance);

        {
          ResourcesContent content(false /* prevent the setting of metadata */);
          content.AddResource(patient, ResourceType_Patient, summary_);
          content.AddResource(study, ResourceType_Study, summary_);
          content.AddResource(series, ResourceType_Series, summary_);
          content.AddResource(instance, ResourceType_Instance, summary_);

          transaction.SetResourcesContent(content);

          ReplaceMetadata(transaction, patient, MetadataType_MainDicomTagsSignature, DicomMap::GetMainDicomTagsSignature(ResourceType_Patient));    // New in Orthanc 1.11.0
          ReplaceMetadata(transaction, study, MetadataType_MainDicomTagsSignature, DicomMap::GetMainDicomTagsSignature(ResourceType_Study));        // New in Orthanc 1.11.0
          ReplaceMetadata(transaction, series, MetadataType_MainDicomTagsSignature, DicomMap::GetMainDicomTagsSignature(ResourceType_Series));      // New in Orthanc 1.11.0
          ReplaceMetadata(transaction, instance, MetadataType_MainDicomTagsSignature, DicomMap::GetMainDicomTagsSignature(ResourceType_Instance));  // New in Orthanc 1.11.0
        }

        if (hasTransferSyntax_)
        {
          ReplaceMetadata(transaction, instance, MetadataType_Instance_TransferSyntax, GetTransferSyntaxUid(transferSyntax_));
        }

        const DicomValue* value;
        if ((value = summary_.TestAndGetValue(DICOM_TAG_SOP_CLASS_UID)) != NULL &&
            !value->IsNull() &&
            !value->IsBinary())
        {
          ReplaceMetadata(transaction, instance, MetadataType_Instance_SopClassUid, value->GetContent());
        }

      }
    };

    Operations operations(dicom);
    Apply(operations);
  }


  static bool IsRecyclingNeeded(IDatabaseWrapper::ITransaction& transaction,
                                uint64_t maximumStorageSize,
                                unsigned int maximumPatients,
                                uint64_t addedInstanceSize)
  {
    if (maximumStorageSize != 0)
    {
      if (maximumStorageSize < addedInstanceSize)
      {
        throw OrthancException(ErrorCode_FullStorage, "Cannot store an instance of size " +
                               boost::lexical_cast<std::string>(addedInstanceSize) +
                               " bytes in a storage area limited to " +
                               boost::lexical_cast<std::string>(maximumStorageSize));
      }
      
      if (transaction.IsDiskSizeAbove(maximumStorageSize - addedInstanceSize))
      {
        return true;
      }
    }

    if (maximumPatients != 0)
    {
      uint64_t patientCount = transaction.GetResourcesCount(ResourceType_Patient);
      if (patientCount > maximumPatients)
      {
        return true;
      }
    }

    return false;
  }
  

  void StatelessDatabaseOperations::ReadWriteTransaction::Recycle(uint64_t maximumStorageSize,
                                                                  unsigned int maximumPatients,
                                                                  uint64_t addedInstanceSize,
                                                                  const std::string& newPatientId)
  {
    // TODO - Performance: Avoid calls to "IsRecyclingNeeded()"
    
    if (IsRecyclingNeeded(transaction_, maximumStorageSize, maximumPatients, addedInstanceSize))
    {
      // Check whether other DICOM instances from this patient are
      // already stored
      int64_t patientToAvoid;
      bool hasPatientToAvoid;

      if (newPatientId.empty())
      {
        hasPatientToAvoid = false;
      }
      else
      {
        ResourceType type;
        hasPatientToAvoid = transaction_.LookupResource(patientToAvoid, type, newPatientId);
        if (type != ResourceType_Patient)
        {
          throw OrthancException(ErrorCode_InternalError);
        }
      }

      // Iteratively select patient to remove until there is enough
      // space in the DICOM store
      int64_t patientToRecycle;
      while (true)
      {
        // If other instances of this patient are already in the store,
        // we must avoid to recycle them
        bool ok = (hasPatientToAvoid ?
                   transaction_.SelectPatientToRecycle(patientToRecycle, patientToAvoid) :
                   transaction_.SelectPatientToRecycle(patientToRecycle));
        
        if (!ok)
        {
          throw OrthancException(ErrorCode_FullStorage, "Cannot recycle more patients");
        }
      
        LOG(TRACE) << "Recycling one patient";
        transaction_.DeleteResource(patientToRecycle);

        if (!IsRecyclingNeeded(transaction_, maximumStorageSize, maximumPatients, addedInstanceSize))
        {
          // OK, we're done
          return;
        }
      }
    }
  }


  void StatelessDatabaseOperations::StandaloneRecycling(uint64_t maximumStorageSize,
                                                        unsigned int maximumPatientCount)
  {
    class Operations : public IReadWriteOperations
    {
    private:
      uint64_t      maximumStorageSize_;
      unsigned int  maximumPatientCount_;
      
    public:
      Operations(uint64_t maximumStorageSize,
                 unsigned int maximumPatientCount) :
        maximumStorageSize_(maximumStorageSize),
        maximumPatientCount_(maximumPatientCount)
      {
      }
        
      virtual void Apply(ReadWriteTransaction& transaction) ORTHANC_OVERRIDE
      {
        transaction.Recycle(maximumStorageSize_, maximumPatientCount_, 0, "");
      }
    };

    if (maximumStorageSize != 0 ||
        maximumPatientCount != 0)
    {
      Operations operations(maximumStorageSize, maximumPatientCount);
      Apply(operations);
    }
  }


  StoreStatus StatelessDatabaseOperations::Store(std::map<MetadataType, std::string>& instanceMetadata,
                                                 const DicomMap& dicomSummary,
                                                 const std::map<DicomTag, Json::Value>& sequencesToStore,
                                                 const Attachments& attachments,
                                                 const MetadataMap& metadata,
                                                 const DicomInstanceOrigin& origin,
                                                 bool overwrite,
                                                 bool hasTransferSyntax,
                                                 DicomTransferSyntax transferSyntax,
                                                 bool hasPixelDataOffset,
                                                 uint64_t pixelDataOffset,
                                                 uint64_t maximumStorageSize,
                                                 unsigned int maximumPatients,
                                                 bool isReconstruct)
  {
    class Operations : public IReadWriteOperations
    {
    private:
      StoreStatus                          storeStatus_;
      std::map<MetadataType, std::string>& instanceMetadata_;
      const DicomMap&                      dicomSummary_;
      const std::map<DicomTag, Json::Value>& sequencesToStore_;
      const Attachments&                   attachments_;
      const MetadataMap&                   metadata_;
      const DicomInstanceOrigin&           origin_;
      bool                                 overwrite_;
      bool                                 hasTransferSyntax_;
      DicomTransferSyntax                  transferSyntax_;
      bool                                 hasPixelDataOffset_;
      uint64_t                             pixelDataOffset_;
      uint64_t                             maximumStorageSize_;
      unsigned int                         maximumPatientCount_;
      bool                                 isReconstruct_;

      // Auto-computed fields
      bool          hasExpectedInstances_;
      int64_t       expectedInstances_;
      std::string   hashPatient_;
      std::string   hashStudy_;
      std::string   hashSeries_;
      std::string   hashInstance_;

      
      static void SetInstanceMetadata(ResourcesContent& content,
                                      std::map<MetadataType, std::string>& instanceMetadata,
                                      int64_t instance,
                                      MetadataType metadata,
                                      const std::string& value)
      {
        content.AddMetadata(instance, metadata, value);
        instanceMetadata[metadata] = value;
      }

      static void SetMainDicomSequenceMetadata(ResourcesContent& content,
                                               int64_t resource,
                                               const std::map<DicomTag, Json::Value>& sequencesToStore,  // all sequences for all levels !
                                               ResourceType level)
      {
        if (sequencesToStore.size() > 0)
        {
          const std::set<DicomTag>& levelTags = DicomMap::GetMainDicomTags(level);
          std::set<DicomTag> levelSequences;
          DicomMap::ExtractSequences(levelSequences, levelTags);

          if (levelSequences.size() == 0)
          {
            return;
          }

          Json::Value jsonMetadata;
          jsonMetadata["Version"] = 1;
          Json::Value jsonSequences = Json::objectValue;

          for (std::set<DicomTag>::const_iterator it = levelSequences.begin();
               it != levelSequences.end(); ++it)
          {
            std::map<DicomTag, Json::Value>::const_iterator foundSeq = sequencesToStore.find(*it);
            if (foundSeq != sequencesToStore.end())
            {
              jsonSequences[it->Format()] = foundSeq->second;
            }
          }
          jsonMetadata["Sequences"] = jsonSequences;
          
          std::string serialized;
          Toolbox::WriteFastJson(serialized, jsonMetadata);

          content.AddMetadata(resource, MetadataType_MainDicomSequences, serialized);
        }

      }
      
      static bool ComputeExpectedNumberOfInstances(int64_t& target,
                                                   const DicomMap& dicomSummary)
      {
        try
        {
          const DicomValue* value;
          const DicomValue* value2;
          
          if ((value = dicomSummary.TestAndGetValue(DICOM_TAG_IMAGES_IN_ACQUISITION)) != NULL &&
              !value->IsNull() &&
              !value->IsBinary() &&
              (value2 = dicomSummary.TestAndGetValue(DICOM_TAG_NUMBER_OF_TEMPORAL_POSITIONS)) != NULL &&
              !value2->IsNull() &&
              !value2->IsBinary())
          {
            // Patch for series with temporal positions thanks to Will Ryder
            int64_t imagesInAcquisition = boost::lexical_cast<int64_t>(value->GetContent());
            int64_t countTemporalPositions = boost::lexical_cast<int64_t>(value2->GetContent());
            target = imagesInAcquisition * countTemporalPositions;
            return (target > 0);
          }

          else if ((value = dicomSummary.TestAndGetValue(DICOM_TAG_NUMBER_OF_SLICES)) != NULL &&
                   !value->IsNull() &&
                   !value->IsBinary() &&
                   (value2 = dicomSummary.TestAndGetValue(DICOM_TAG_NUMBER_OF_TIME_SLICES)) != NULL &&
                   !value2->IsBinary() &&
                   !value2->IsNull())
          {
            // Support of Cardio-PET images
            int64_t numberOfSlices = boost::lexical_cast<int64_t>(value->GetContent());
            int64_t numberOfTimeSlices = boost::lexical_cast<int64_t>(value2->GetContent());
            target = numberOfSlices * numberOfTimeSlices;
            return (target > 0);
          }

          else if ((value = dicomSummary.TestAndGetValue(DICOM_TAG_CARDIAC_NUMBER_OF_IMAGES)) != NULL &&
                   !value->IsNull() &&
                   !value->IsBinary())
          {
            target = boost::lexical_cast<int64_t>(value->GetContent());
            return (target > 0);
          }
        }
        catch (OrthancException&)
        {
        }
        catch (boost::bad_lexical_cast&)
        {
        }

        return false;
      }

    public:
      Operations(std::map<MetadataType, std::string>& instanceMetadata,
                 const DicomMap& dicomSummary,
                 const std::map<DicomTag, Json::Value>& sequencesToStore,
                 const Attachments& attachments,
                 const MetadataMap& metadata,
                 const DicomInstanceOrigin& origin,
                 bool overwrite,
                 bool hasTransferSyntax,
                 DicomTransferSyntax transferSyntax,
                 bool hasPixelDataOffset,
                 uint64_t pixelDataOffset,
                 uint64_t maximumStorageSize,
                 unsigned int maximumPatientCount,
                 bool isReconstruct) :
        storeStatus_(StoreStatus_Failure),
        instanceMetadata_(instanceMetadata),
        dicomSummary_(dicomSummary),
        sequencesToStore_(sequencesToStore),
        attachments_(attachments),
        metadata_(metadata),
        origin_(origin),
        overwrite_(overwrite),
        hasTransferSyntax_(hasTransferSyntax),
        transferSyntax_(transferSyntax),
        hasPixelDataOffset_(hasPixelDataOffset),
        pixelDataOffset_(pixelDataOffset),
        maximumStorageSize_(maximumStorageSize),
        maximumPatientCount_(maximumPatientCount),
        isReconstruct_(isReconstruct)
      {
        hasExpectedInstances_ = ComputeExpectedNumberOfInstances(expectedInstances_, dicomSummary);
    
        instanceMetadata_.clear();

        DicomInstanceHasher hasher(dicomSummary);
        hashPatient_ = hasher.HashPatient();
        hashStudy_ = hasher.HashStudy();
        hashSeries_ = hasher.HashSeries();
        hashInstance_ = hasher.HashInstance();
      }

      StoreStatus GetStoreStatus() const
      {
        return storeStatus_;
      }
        
      virtual void Apply(ReadWriteTransaction& transaction) ORTHANC_OVERRIDE
      {
        try
        {
          IDatabaseWrapper::CreateInstanceResult status;
          int64_t instanceId;

          // Check whether this instance is already stored
          if (!transaction.CreateInstance(status, instanceId, hashPatient_,
                                          hashStudy_, hashSeries_, hashInstance_))
          {
            // The instance already exists
        
            if (overwrite_)
            {
              // Overwrite the old instance
              LOG(INFO) << "Overwriting instance: " << hashInstance_;
              transaction.DeleteResource(instanceId);

              // Re-create the instance, now that the old one is removed
              if (!transaction.CreateInstance(status, instanceId, hashPatient_,
                                              hashStudy_, hashSeries_, hashInstance_))
              {
                throw OrthancException(ErrorCode_InternalError);
              }
            }
            else
            {
              // Do nothing if the instance already exists and overwriting is disabled
              transaction.GetAllMetadata(instanceMetadata_, instanceId);
              storeStatus_ = StoreStatus_AlreadyStored;
              return;
            }
          }


          if (!isReconstruct_)  // don't signal new resources if this is a reconstruction
          {
            // Warn about the creation of new resources. The order must be
            // from instance to patient.

            // NB: In theory, could be sped up by grouping the underlying
            // calls to "transaction.LogChange()". However, this would only have an
            // impact when new patient/study/series get created, which
            // occurs far less often that creating new instances. The
            // positive impact looks marginal in practice.
            transaction.LogChange(instanceId, ChangeType_NewInstance, ResourceType_Instance, hashInstance_);

            if (status.isNewSeries_)
            {
              transaction.LogChange(status.seriesId_, ChangeType_NewSeries, ResourceType_Series, hashSeries_);
            }
        
            if (status.isNewStudy_)
            {
              transaction.LogChange(status.studyId_, ChangeType_NewStudy, ResourceType_Study, hashStudy_);
            }
        
            if (status.isNewPatient_)
            {
              transaction.LogChange(status.patientId_, ChangeType_NewPatient, ResourceType_Patient, hashPatient_);
            }
          }      
      
          // Ensure there is enough room in the storage for the new instance
          uint64_t instanceSize = 0;
          for (Attachments::const_iterator it = attachments_.begin();
               it != attachments_.end(); ++it)
          {
            instanceSize += it->GetCompressedSize();
          }

          if (!isReconstruct_)  // reconstruction should not affect recycling
          {
            transaction.Recycle(maximumStorageSize_, maximumPatientCount_,
                                instanceSize, hashPatient_ /* don't consider the current patient for recycling */);
          }  
     
          // Attach the files to the newly created instance
          for (Attachments::const_iterator it = attachments_.begin();
               it != attachments_.end(); ++it)
          {
            transaction.AddAttachment(instanceId, *it, 0 /* this is the first revision */);
          }

      
          {
            ResourcesContent content(true /* new resource, metadata can be set */);


            // Attach the user-specified metadata (in case of reconstruction, metadata_ contains all past metadata, including the system ones we want to keep)
            for (MetadataMap::const_iterator 
                   it = metadata_.begin(); it != metadata_.end(); ++it)
            {
              switch (it->first.first)
              {
                case ResourceType_Patient:
                  content.AddMetadata(status.patientId_, it->first.second, it->second);
                  break;

                case ResourceType_Study:
                  content.AddMetadata(status.studyId_, it->first.second, it->second);
                  break;

                case ResourceType_Series:
                  content.AddMetadata(status.seriesId_, it->first.second, it->second);
                  break;

                case ResourceType_Instance:
                  SetInstanceMetadata(content, instanceMetadata_, instanceId,
                                      it->first.second, it->second);
                  break;

                default:
                  throw OrthancException(ErrorCode_ParameterOutOfRange);
              }
            }

            // Populate the tags of the newly-created resources

            content.AddResource(instanceId, ResourceType_Instance, dicomSummary_);
            SetInstanceMetadata(content, instanceMetadata_, instanceId, MetadataType_MainDicomTagsSignature, DicomMap::GetMainDicomTagsSignature(ResourceType_Instance));  // New in Orthanc 1.11.0
            SetMainDicomSequenceMetadata(content, instanceId, sequencesToStore_, ResourceType_Instance);   // new in Orthanc 1.11.1

            if (status.isNewSeries_)
            {
              content.AddResource(status.seriesId_, ResourceType_Series, dicomSummary_);
              content.AddMetadata(status.seriesId_, MetadataType_MainDicomTagsSignature, DicomMap::GetMainDicomTagsSignature(ResourceType_Series));  // New in Orthanc 1.11.0
              SetMainDicomSequenceMetadata(content, status.seriesId_, sequencesToStore_, ResourceType_Series);   // new in Orthanc 1.11.1
            }

            if (status.isNewStudy_)
            {
              content.AddResource(status.studyId_, ResourceType_Study, dicomSummary_);
              content.AddMetadata(status.studyId_, MetadataType_MainDicomTagsSignature, DicomMap::GetMainDicomTagsSignature(ResourceType_Study));  // New in Orthanc 1.11.0
              SetMainDicomSequenceMetadata(content, status.studyId_, sequencesToStore_, ResourceType_Study);   // new in Orthanc 1.11.1
            }

            if (status.isNewPatient_)
            {
              content.AddResource(status.patientId_, ResourceType_Patient, dicomSummary_);
              content.AddMetadata(status.patientId_, MetadataType_MainDicomTagsSignature, DicomMap::GetMainDicomTagsSignature(ResourceType_Patient));  // New in Orthanc 1.11.0
              SetMainDicomSequenceMetadata(content, status.patientId_, sequencesToStore_, ResourceType_Patient);   // new in Orthanc 1.11.1
            }

            // Attach the auto-computed metadata for the patient/study/series levels
            std::string now = SystemToolbox::GetNowIsoString(true /* use UTC time (not local time) */);
            content.AddMetadata(status.seriesId_, MetadataType_LastUpdate, now);
            content.AddMetadata(status.studyId_, MetadataType_LastUpdate, now);
            content.AddMetadata(status.patientId_, MetadataType_LastUpdate, now);

            if (status.isNewSeries_)
            {
              if (hasExpectedInstances_)
              {
                content.AddMetadata(status.seriesId_, MetadataType_Series_ExpectedNumberOfInstances,
                                    boost::lexical_cast<std::string>(expectedInstances_));
              }

              // New in Orthanc 1.9.0
              content.AddMetadata(status.seriesId_, MetadataType_RemoteAet,
                                  origin_.GetRemoteAetC());
            }

            if (hasTransferSyntax_)
            {
              // New in Orthanc 1.2.0
              SetInstanceMetadata(content, instanceMetadata_, instanceId,
                                  MetadataType_Instance_TransferSyntax,
                                  GetTransferSyntaxUid(transferSyntax_));
            }

            if (!isReconstruct_) // don't change origin metadata
            {        
              // Attach the auto-computed metadata for the instance level,
              // reflecting these additions into the input metadata map
              SetInstanceMetadata(content, instanceMetadata_, instanceId,
                                  MetadataType_Instance_ReceptionDate, now);
              SetInstanceMetadata(content, instanceMetadata_, instanceId, MetadataType_RemoteAet,
                                  origin_.GetRemoteAetC());
              SetInstanceMetadata(content, instanceMetadata_, instanceId, MetadataType_Instance_Origin, 
                                  EnumerationToString(origin_.GetRequestOrigin()));

              std::string s;

              if (origin_.LookupRemoteIp(s))
              {
                // New in Orthanc 1.4.0
                SetInstanceMetadata(content, instanceMetadata_, instanceId,
                                    MetadataType_Instance_RemoteIp, s);
              }

              if (origin_.LookupCalledAet(s))
              {
                // New in Orthanc 1.4.0
                SetInstanceMetadata(content, instanceMetadata_, instanceId,
                                    MetadataType_Instance_CalledAet, s);
              }

              if (origin_.LookupHttpUsername(s))
              {
                // New in Orthanc 1.4.0
                SetInstanceMetadata(content, instanceMetadata_, instanceId,
                                    MetadataType_Instance_HttpUsername, s);
              }
            }

            if (hasPixelDataOffset_)
            {
              // New in Orthanc 1.9.1
              SetInstanceMetadata(content, instanceMetadata_, instanceId,
                                  MetadataType_Instance_PixelDataOffset,
                                  boost::lexical_cast<std::string>(pixelDataOffset_));
            }
        
            const DicomValue* value;
            if ((value = dicomSummary_.TestAndGetValue(DICOM_TAG_SOP_CLASS_UID)) != NULL &&
                !value->IsNull() &&
                !value->IsBinary())
            {
              SetInstanceMetadata(content, instanceMetadata_, instanceId,
                                  MetadataType_Instance_SopClassUid, value->GetContent());
            }


            if ((value = dicomSummary_.TestAndGetValue(DICOM_TAG_INSTANCE_NUMBER)) != NULL ||
                (value = dicomSummary_.TestAndGetValue(DICOM_TAG_IMAGE_INDEX)) != NULL)
            {
              if (!value->IsNull() && 
                  !value->IsBinary())
              {
                SetInstanceMetadata(content, instanceMetadata_, instanceId,
                                    MetadataType_Instance_IndexInSeries, Toolbox::StripSpaces(value->GetContent()));
              }
            }

        
            transaction.SetResourcesContent(content);
          }

  
          // Check whether the series of this new instance is now completed
          int64_t expectedNumberOfInstances;
          if (ComputeExpectedNumberOfInstances(expectedNumberOfInstances, dicomSummary_))
          {
            SeriesStatus seriesStatus = transaction.GetSeriesStatus(status.seriesId_, expectedNumberOfInstances);
            if (seriesStatus == SeriesStatus_Complete)
            {
              transaction.LogChange(status.seriesId_, ChangeType_CompletedSeries, ResourceType_Series, hashSeries_);
            }
          }
          
          transaction.LogChange(status.seriesId_, ChangeType_NewChildInstance, ResourceType_Series, hashSeries_);
          transaction.LogChange(status.studyId_, ChangeType_NewChildInstance, ResourceType_Study, hashStudy_);
          transaction.LogChange(status.patientId_, ChangeType_NewChildInstance, ResourceType_Patient, hashPatient_);
          
          // Mark the parent resources of this instance as unstable
          transaction.GetTransactionContext().MarkAsUnstable(status.seriesId_, ResourceType_Series, hashSeries_);
          transaction.GetTransactionContext().MarkAsUnstable(status.studyId_, ResourceType_Study, hashStudy_);
          transaction.GetTransactionContext().MarkAsUnstable(status.patientId_, ResourceType_Patient, hashPatient_);
          transaction.GetTransactionContext().SignalAttachmentsAdded(instanceSize);

          storeStatus_ = StoreStatus_Success;          
        }
        catch (OrthancException& e)
        {
          if (e.GetErrorCode() == ErrorCode_DatabaseCannotSerialize)
          {
            throw;  // the transaction has failed -> do not commit the current transaction (and retry)
          }
          else
          {
            LOG(ERROR) << "EXCEPTION [" << e.What() << " - " << e.GetDetails() << "]";

            if (e.GetErrorCode() == ErrorCode_FullStorage)
            {
              throw; // do not commit the current transaction
            }

            // this is an expected failure, exit normaly and commit the current transaction
            storeStatus_ = StoreStatus_Failure;
          }
        }
      }
    };


    Operations operations(instanceMetadata, dicomSummary, sequencesToStore, attachments, metadata, origin,
                          overwrite, hasTransferSyntax, transferSyntax, hasPixelDataOffset,
                          pixelDataOffset, maximumStorageSize, maximumPatients, isReconstruct);
    Apply(operations);
    return operations.GetStoreStatus();
  }


  StoreStatus StatelessDatabaseOperations::AddAttachment(int64_t& newRevision,
                                                         const FileInfo& attachment,
                                                         const std::string& publicId,
                                                         uint64_t maximumStorageSize,
                                                         unsigned int maximumPatients,
                                                         bool hasOldRevision,
                                                         int64_t oldRevision,
                                                         const std::string& oldMD5)
  {
    class Operations : public IReadWriteOperations
    {
    private:
      int64_t&            newRevision_;
      StoreStatus         status_;
      const FileInfo&     attachment_;
      const std::string&  publicId_;
      uint64_t            maximumStorageSize_;
      unsigned int        maximumPatientCount_;
      bool                hasOldRevision_;
      int64_t             oldRevision_;
      const std::string&  oldMD5_;

    public:
      Operations(int64_t& newRevision,
                 const FileInfo& attachment,
                 const std::string& publicId,
                 uint64_t maximumStorageSize,
                 unsigned int maximumPatientCount,
                 bool hasOldRevision,
                 int64_t oldRevision,
                 const std::string& oldMD5) :
        newRevision_(newRevision),
        status_(StoreStatus_Failure),
        attachment_(attachment),
        publicId_(publicId),
        maximumStorageSize_(maximumStorageSize),
        maximumPatientCount_(maximumPatientCount),
        hasOldRevision_(hasOldRevision),
        oldRevision_(oldRevision),
        oldMD5_(oldMD5)
      {
      }

      StoreStatus GetStatus() const
      {
        return status_;
      }
        
      virtual void Apply(ReadWriteTransaction& transaction) ORTHANC_OVERRIDE
      {
        ResourceType resourceType;
        int64_t resourceId;
        if (!transaction.LookupResource(resourceId, resourceType, publicId_))
        {
          status_ = StoreStatus_Failure;  // Inexistent resource
        }
        else
        {
          // Possibly remove previous attachment
          {
            FileInfo oldFile;
            int64_t expectedRevision;
            if (transaction.LookupAttachment(oldFile, expectedRevision, resourceId, attachment_.GetContentType()))
            {
              if (hasOldRevision_ &&
                  (expectedRevision != oldRevision_ ||
                   oldFile.GetUncompressedMD5() != oldMD5_))
              {
                throw OrthancException(ErrorCode_Revision);
              }
              else
              {
                newRevision_ = expectedRevision + 1;
                transaction.DeleteAttachment(resourceId, attachment_.GetContentType());
              }
            }
            else
            {
              // The attachment is not existing yet: Ignore "oldRevision"
              // and initialize a new sequence of revisions
              newRevision_ = 0;
            }
          }

          // Locate the patient of the target resource
          int64_t patientId = resourceId;
          for (;;)
          {
            int64_t parent;
            if (transaction.LookupParent(parent, patientId))
            {
              // We have not reached the patient level yet
              patientId = parent;
            }
            else
            {
              // We have reached the patient level
              break;
            }
          }

          // Possibly apply the recycling mechanism while preserving this patient
          assert(transaction.GetResourceType(patientId) == ResourceType_Patient);
          transaction.Recycle(maximumStorageSize_, maximumPatientCount_,
                              attachment_.GetCompressedSize(), transaction.GetPublicId(patientId));

          transaction.AddAttachment(resourceId, attachment_, newRevision_);

          if (IsUserContentType(attachment_.GetContentType()))
          {
            transaction.LogChange(resourceId, ChangeType_UpdatedAttachment, resourceType, publicId_);
          }

          transaction.GetTransactionContext().SignalAttachmentsAdded(attachment_.GetCompressedSize());

          status_ = StoreStatus_Success;
        }
      }
    };


    Operations operations(newRevision, attachment, publicId, maximumStorageSize, maximumPatients,
                          hasOldRevision, oldRevision, oldMD5);
    Apply(operations);
    return operations.GetStatus();
  }
}
