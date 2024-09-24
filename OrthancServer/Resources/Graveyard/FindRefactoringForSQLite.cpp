#if 0
    // TODO-FIND: Remove this implementation, as it should be done by
    // the compatibility mode implemented by "GenericFind"
    
    virtual void ExecuteFind(FindResponse& response,
                             const FindRequest& request, 
                             const std::vector<DatabaseConstraint>& normalized) ORTHANC_OVERRIDE
    {
#if 0
      Compatibility::GenericFind find(*this);
      find.Execute(response, request);
#else
      {
        SQLite::Statement s(db_, SQLITE_FROM_HERE, "DROP TABLE IF EXISTS FilteredResourcesIds");
        s.Run();
      }

      {

        LookupFormatter formatter;

        std::string sqlLookup;
        LookupFormatter::Apply(sqlLookup, 
                               formatter, 
                               normalized, 
                               request.GetLevel(),
                               request.GetLabels(),
                               request.GetLabelsConstraint(),
                               (request.HasLimits() ? request.GetLimitsCount() : 0));  // TODO: handles since and count

        {
          // first create a temporary table that with the filtered and ordered results
          sqlLookup = "CREATE TEMPORARY TABLE FilteredResourcesIds AS " + sqlLookup;

          SQLite::Statement statement(db_, SQLITE_FROM_HERE_DYNAMIC(sqlLookup), sqlLookup);
          formatter.Bind(statement);
          statement.Run();
        }

        {
          // create the response item with the public ids only
          SQLite::Statement statement(db_, SQLITE_FROM_HERE, "SELECT publicId FROM FilteredResourcesIds");
          formatter.Bind(statement);

          while (statement.Step())
          {
            const std::string resourceId = statement.ColumnString(0);
            response.Add(new FindResponse::Resource(request.GetLevel(), resourceId));
          }
        }

        // request Each response content through INNER JOIN with the temporary table
        if (request.IsRetrieveMainDicomTags())
        {
          // TODO-FIND: handle the case where we request tags from multiple levels
          SQLite::Statement statement(db_, SQLITE_FROM_HERE, 
                                      "SELECT publicId, tagGroup, tagElement, value FROM MainDicomTags AS tags "
                                      "  INNER JOIN FilteredResourcesIds  ON tags.id = FilteredResourcesIds.internalId");
          formatter.Bind(statement);

          while (statement.Step())
          {
            const std::string& resourceId = statement.ColumnString(0);
            assert(response.HasResource(resourceId));
            response.GetResource(resourceId).AddStringDicomTag(statement.ColumnInt(1),
                                                               statement.ColumnInt(2),
                                                               statement.ColumnString(3));
          }
        }

        if (request.IsRetrieveChildrenIdentifiers())
        {
          SQLite::Statement statement(db_, SQLITE_FROM_HERE, 
                                      "SELECT filtered.publicId, childLevel.publicId AS childPublicId "
                                      "FROM Resources as currentLevel "
                                      "    INNER JOIN FilteredResourcesIds filtered ON filtered.internalId = currentLevel.internalId "
                                      "    INNER JOIN Resources childLevel ON childLevel.parentId = currentLevel.internalId");
          formatter.Bind(statement);

          while (statement.Step())
          {
            const std::string& resourceId = statement.ColumnString(0);
            assert(response.HasResource(resourceId));
            response.GetResource(resourceId).AddChildIdentifier(GetChildResourceType(request.GetLevel()), statement.ColumnString(1));
          }
        }

        if (request.IsRetrieveParentIdentifier())
        {
          SQLite::Statement statement(db_, SQLITE_FROM_HERE, 
                                      "SELECT filtered.publicId, parentLevel.publicId AS parentPublicId "
                                      "FROM Resources as currentLevel "
                                      "    INNER JOIN FilteredResourcesIds filtered ON filtered.internalId = currentLevel.internalId "
                                      "    INNER JOIN Resources parentLevel ON currentLevel.parentId = parentLevel.internalId");

          while (statement.Step())
          {
            const std::string& resourceId = statement.ColumnString(0);
            const std::string& parentId = statement.ColumnString(1);
            assert(response.HasResource(resourceId));
            response.GetResource(resourceId).SetParentIdentifier(parentId);
          }
        }

        if (request.IsRetrieveMetadata())
        {
          SQLite::Statement statement(db_, SQLITE_FROM_HERE, 
                                      "SELECT filtered.publicId, metadata.type, metadata.value "
                                      "FROM Metadata "
                                      "  INNER JOIN FilteredResourcesIds filtered ON filtered.internalId = Metadata.id");

          while (statement.Step())
          {
            const std::string& resourceId = statement.ColumnString(0);
            assert(response.HasResource(resourceId));
            response.GetResource(resourceId).AddMetadata(static_cast<MetadataType>(statement.ColumnInt(1)),
                                                         statement.ColumnString(2));
          }
        }

        if (request.IsRetrieveLabels())
        {
          SQLite::Statement statement(db_, SQLITE_FROM_HERE, 
                                      "SELECT filtered.publicId, label "
                                      "FROM Labels "
                                      "  INNER JOIN FilteredResourcesIds filtered ON filtered.internalId = Labels.id");

          while (statement.Step())
          {
            const std::string& resourceId = statement.ColumnString(0);
            assert(response.HasResource(resourceId));
            response.GetResource(resourceId).AddLabel(statement.ColumnString(1));
          }
        }

        if (request.IsRetrieveAttachments())
        {
          SQLite::Statement statement(db_, SQLITE_FROM_HERE, 
                                      "SELECT filtered.publicId, uuid, fileType, uncompressedSize, compressionType, compressedSize, "
                                      "       uncompressedMD5, compressedMD5 "
                                      "FROM AttachedFiles "
                                      "  INNER JOIN FilteredResourcesIds filtered ON filtered.internalId = AttachedFiles.id");

          while (statement.Step())
          {
            const std::string& resourceId = statement.ColumnString(0);
            FileInfo attachment = FileInfo(statement.ColumnString(1),
                                           static_cast<FileContentType>(statement.ColumnInt(2)),
                                           statement.ColumnInt64(3),
                                           statement.ColumnString(6),
                                           static_cast<CompressionType>(statement.ColumnInt(4)),
                                           statement.ColumnInt64(5),
                                           statement.ColumnString(7));

            assert(response.HasResource(resourceId));
            response.GetResource(resourceId).AddAttachment(attachment);
          };
        }

        // TODO-FIND: implement other responseContent: ResponseContent_ChildInstanceId, ResponseContent_ChildrenMetadata (later: ResponseContent_IsStable)

      }

#endif
    }
#endif

