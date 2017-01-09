/*
 * Copyright (C) by Piotr Mrowczynski <piotr@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include "propagateupload.h"

namespace OCC {


qint64 PropagateNormalUpload::committedDiskSpace() const
{
    qint64 needed = 0;
    foreach (PropagatorJob* job, _dbJobs) {
        needed += job->committedDiskSpace();
    }
    foreach (PropagatorJob* job, _standardJobs) {
        needed += job->committedDiskSpace();
    }
    return needed;
}

bool PropagateNormalUpload::scheduleNextJobRoutine(QVector<PropagatorJob *> &subJobs) {
    QMutableVectorIterator<PropagatorJob *> subJobsIterator(subJobs);
    while (subJobsIterator.hasNext()) {
        subJobsIterator.next();
        // Get the state of the sub job pointed at by call next()
        if (subJobsIterator.value()->_state == Finished) {
            // If this item is finished, remove it from the _subJobs list as it is not needed anymore
            // Note that in this case remove() from QVector will just perform memmove of pointer array items.
            PropagatorJob * job = subJobsIterator.value();
            subJobsIterator.remove();

            // In this case we dont delete the job, but save it in the _finishedSubJobs queue
            // We might need this job in slotSubJobFinished
            // The PropagateNormalUpload class will be destroyed in PropagateDirectory
            // when it will detect that we finished PropagateNormalUpload
            _finishedSubJobs.append(job);
            continue;
        }

        if (possiblyRunNextJob(subJobsIterator.value())) {
            return true;
        }

        Q_ASSERT(subJobsIterator.value()->_state == Running);
    }
    return false;
}

void PropagateNormalUpload::append(const SyncFileItemPtr &item) {
    // In case of bundles, in here we should append BundledUpload jobs with new files to the .top() of _subJobs until chunking size is reached.
    // The role of this class is also to control how much data is going into the container class.
    // In version 1.0 append just PUTs

    PropagateUploadFileV1* subJob = new PropagateUploadFileV1(_propagator, item);
    if (item->_size <= _propagator->smallFileSize()){
        _propagator->_dbJobsCount++; // Db operations for this item take considerably longer then any other factors.
        _dbJobs.append(subJob);
    } else {
        _propagator->_standardJobsCount++; // This item is not a small upload file, so it is standard.
        _standardJobs.append(subJob);
    }
}

bool PropagateNormalUpload::scheduleNextJob()
{
    if (_state == Finished) {
        return false;
    }

    if (_state == NotYetStarted) {
        _state = Running;

        if (_dbJobs.isEmpty() && _standardJobs.isEmpty()) {
            finalize();
            return true;
        }

        // At the beginning of the Directory Job, update the expected number of Jobs to be synced
        _totalJobs = _standardJobs.count() + _dbJobs.count();
    }

    // Check if there are standard jobs in the whole sync waiting for sync or pending
    if (_propagator->_standardJobsCount > 0){
        // Check if there are already some dbJobs running
        if(_propagator->_activeDBJobs > 0){
            // Run standardJobs represented by _propagator->_standardJobsCount
            return scheduleNextJobRoutine(_standardJobs);
        } else {
            // There is no running dbJob, try to schedule one
            if(scheduleNextJobRoutine(_dbJobs)){
                // This container contains dbJobs and will sync it
                _propagator->_activeDBJobs++;
                return true;
            } else {
                // This container does not contain any remaining dbJobs
                if(_runningNow > 1){
                    // There are some jobs running in this container,
                    // but they are not dbJobs, so search in a different container for db jobs
                    return false;
                }
                return scheduleNextJobRoutine(_standardJobs);
            }
        }
    } else {
        // There are no remaining or pending standard jobs in the whole sync
        // This also means that _standardJobs is empty
        Q_ASSERT(!scheduleNextJobRoutine(_standardJobs));

        // Parallelise itself into more flows flows
        if(scheduleNextJobRoutine(_dbJobs)){
            _propagator->_activeDBJobs++;
            return true;
        }
        return false;
    }
}


void PropagateNormalUpload::slotSubJobFinished(SyncFileItem::Status status)
{
    if (status == SyncFileItem::FatalError) {
        abort();
        _state = Finished;
        emit finished(status);
        return;
    } else if (status == SyncFileItem::NormalError || status == SyncFileItem::SoftError) {
        _hasError = status;
    }

    // We need to ensure that this PropagateItemJob exists by not deleting it prematurely
    PropagateItemJob *job = qobject_cast<PropagateItemJob *>(sender());
    Q_ASSERT(job);

    // Reduce the global counter of db or standard jobs
    if (job->_item->_size <= _propagator->smallFileSize()){
        _propagator->_dbJobsCount--;
        _propagator->_activeDBJobs--;
    } else {
        _propagator->_standardJobsCount--; // This item is not a small upload file, so it is standard
    }

    _runningNow--;
    _jobsFinished++;

    // We finished processing all the jobs
    // check if we finished
    if (_jobsFinished >= _totalJobs) {
        Q_ASSERT(_propagator->_standardJobsCount>=0);
        Q_ASSERT(!_runningNow); // how can we be finished if there are still jobs running now
        finalize();
    } else {
        emit ready();
    }
}

void PropagateNormalUpload::finalize()
{
    _state = Finished;
    emit finished(_hasError == SyncFileItem::NoStatus ?  SyncFileItem::Success : _hasError);
}

}
