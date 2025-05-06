// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "lookup_list_import_processor.h"

#include <QtConcurrent/QtConcurrentRun>
#include <QtCore/QFutureWatcher>
#include <QtCore/QMap>
#include <QtCore/QScopedPointer>
#include <QtCore/QString>

#include <ui/dialogs/common/custom_file_dialog.h>

namespace nx::vms::client::desktop {

class LookupListImportProcessor::Private: public QObject
{
    LookupListImportProcessor* const q = nullptr;

public:
    Private(LookupListImportProcessor* q);
    void processImportTaskResult();
    void processFixupTaskResult();
    static ImportExitCode runImportTask(const QString sourceFile,
        const QString separator,
        bool importHeaders,
        LookupListImportEntriesModel* model);
    static ImportExitCode runFixupTask(LookupListImportEntriesModel* model);

    QFutureWatcher<ImportExitCode> currentImportTask;
    QFutureWatcher<ImportExitCode> currentFixupTask;

    bool importConnectionInitialized = false;
    bool fixupConnectionInitialized = false;
};

LookupListImportProcessor::Private::Private(LookupListImportProcessor* q): QObject(), q(q)
{
}

LookupListImportProcessor::ImportExitCode LookupListImportProcessor::Private::runFixupTask(
    LookupListImportEntriesModel* model)
{
    if (!NX_ASSERT(model))
        return InternalError;

    auto lookuplistEntriesModel = model->lookupListEntriesModel();
    {
        // Need to block signals from model, to avoid repainting of the table on each editing of
        // row.
        QSignalBlocker listEntriesModelBlocker(lookuplistEntriesModel);
        model->applyFix();
    }
    lookuplistEntriesModel->update();
    return model->hasImportedRows() ? Success : SuccessEmptyImport;
}

/**
 * Parses and validates values from CSV files. Correct parsed result is added to the input model.
 */
LookupListImportProcessor::ImportExitCode LookupListImportProcessor::Private::runImportTask(
    const QString sourceFile,
    const QString separator,
    bool importHeaders,
    LookupListImportEntriesModel* model)
{
    if (!NX_ASSERT(model))
        return InternalError;

    QFile file(sourceFile);
    if (!file.open(QFile::ReadOnly))
        return ErrorFileNotFound;

    auto columnIndexToAttribute = model->columnIndexToAttribute();
    if (columnIndexToAttribute.isEmpty())
        return SuccessEmptyImport; //< User have chosen not to import any columns.

    QTextStream streamCsv(&file);
    if (importHeaders)
        streamCsv.readLine(); //< Just skip headers if them are in file.

    auto lookuplistEntriesModel = model->lookupListEntriesModel();
    const int initialRowCount = lookuplistEntriesModel->rowCount();
    {
        // Need to block signals from models, to avoid repainting of table on each addition of row.
        QSignalBlocker importModelBlocker(model);
        QSignalBlocker listEntriesModelBlocker(lookuplistEntriesModel);

        for (auto line = streamCsv.readLine(); !line.isEmpty(); line = streamCsv.readLine())
        {
            const auto words = line.split(separator);
            QVariantMap entry;
            for (int columnIndex = 0; columnIndex < model->columnCount(); ++columnIndex)
            {
                if (columnIndex >= words.size())
                    break;

                const auto columnIndexToAttributeIter = columnIndexToAttribute.find(columnIndex);
                if (columnIndexToAttributeIter == columnIndexToAttribute.end())
                    continue; //< "Do not import" was chosen.

                entry[columnIndexToAttributeIter.value()] = words[columnIndex];
            }

            model->addLookupListEntry(entry);
        }
    }

    // Manually emitting signals from models.
    if (model->fixupRequired())
    {
        emit model->fixupDataChanged();
        return ClarificationRequired;
    }

    if (initialRowCount != lookuplistEntriesModel->rowCount())
        lookuplistEntriesModel->update();

    return Success;
}

void LookupListImportProcessor::Private::processImportTaskResult()
{
    emit q->importFinished(currentImportTask.isCanceled() ? Canceled : currentImportTask.result());
}

void LookupListImportProcessor::Private::processFixupTaskResult()
{
    emit q->fixupFinished(currentFixupTask.isCanceled() ? Canceled : currentFixupTask.result());
}

// ------------------------------------------------------------------------------------------------------

LookupListImportProcessor::LookupListImportProcessor(QObject* parent):
    base_type(parent), d(new Private(this))
{
}

LookupListImportProcessor::~LookupListImportProcessor()
{
}

void LookupListImportProcessor::cancelRunningTask()
{
    if (d->currentImportTask.isRunning())
        d->currentImportTask.cancel();
    if (d->currentFixupTask.isRunning())
        d->currentFixupTask.cancel();
}

bool LookupListImportProcessor::importListEntries(const QString sourceFile,
    const QString separator,
    const bool importHeaders,
    LookupListImportEntriesModel* model)
{
    if (d->currentFixupTask.isRunning() || d->currentImportTask.isRunning())
        return false; //< Import and fixup tasks can't be run simultaneously.

    if (sourceFile.isEmpty() || separator.isEmpty())
        return false;

    if (!d->importConnectionInitialized)
    {
        connect(&d->currentImportTask,
            &QFutureWatcherBase::started,
            this,
            &LookupListImportProcessor::importStarted);
        connect(&d->currentImportTask,
            &QFutureWatcherBase::canceled,
            d.get(),
            &Private::processImportTaskResult);
        connect(&d->currentImportTask,
            &QFutureWatcherBase::finished,
            d.get(),
            &Private::processImportTaskResult);

        d->importConnectionInitialized = true;
    }

    auto future =
        QtConcurrent::run(Private::runImportTask, sourceFile, separator, importHeaders, model);
    d->currentImportTask.setFuture(future);

    return true;
}

bool LookupListImportProcessor::applyFixUps(LookupListImportEntriesModel* model)
{
    if (d->currentFixupTask.isRunning() || d->currentImportTask.isRunning())
        return false; //< Import and fixup tasks can't be run simultaneously.

    if (!d->fixupConnectionInitialized)
    {
        connect(&d->currentFixupTask,
            &QFutureWatcherBase::started,
            this,
            &LookupListImportProcessor::fixupStarted);
        connect(&d->currentFixupTask,
            &QFutureWatcherBase::canceled,
            d.get(),
            &Private::processFixupTaskResult);
        connect(&d->currentFixupTask,
            &QFutureWatcherBase::finished,
            d.get(),
            &Private::processFixupTaskResult);

        d->fixupConnectionInitialized = true;
    }

    auto future = QtConcurrent::run(Private::runFixupTask, model);
    d->currentFixupTask.setFuture(future);

    return true;
}

} // namespace nx::vms::client::desktop
