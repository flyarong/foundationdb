/*
 * RestoreWorkerInterface.actor.h
 *
 * This source file is part of the FoundationDB open source project
 *
 * Copyright 2013-2018 Apple Inc. and the FoundationDB project authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// This file declare and define the interface for RestoreWorker and restore roles
// which are RestoreMaster, RestoreLoader, and RestoreApplier


#pragma once
#if defined(NO_INTELLISENSE) && !defined(FDBSERVER_RESTORE_WORKER_INTERFACE_ACTOR_G_H)
	#define FDBSERVER_RESTORE_WORKER_INTERFACE_ACTOR_G_H
	#include "fdbserver/RestoreWorkerInterface.actor.g.h"
#elif !defined(FDBSERVER_RESTORE_WORKER_INTERFACE_ACTOR_H)
	#define FDBSERVER_RESTORE_WORKER_INTERFACE_ACTOR_H


#include <sstream>
#include "flow/Stats.h"
#include "fdbrpc/fdbrpc.h"
#include "fdbrpc/Locality.h"
#include "fdbclient/FDBTypes.h"
#include "fdbclient/CommitTransaction.h"
#include "fdbserver/CoordinationInterface.h"
#include "fdbserver/Knobs.h"
#include "fdbserver/RestoreUtil.h"

#include "flow/actorcompiler.h" // has to be last include

#define DUMPTOKEN( name ) TraceEvent("DumpToken", recruited.id()).detail("Name", #name).detail("Token", name.getEndpoint().token)

class RestoreConfig;

struct RestoreCommonReply;
struct RestoreRecruitRoleRequest;
struct RestoreSysInfoRequest;
struct RestoreLoadFileRequest;
struct RestoreVersionBatchRequest;
struct RestoreSendMutationVectorVersionedRequest;
struct RestoreSetApplierKeyRangeVectorRequest;
struct RestoreSysInfo;
struct RestoreApplierInterface;


// RestoreSysInfo includes information each (type of) restore roles should know.
// At this moment, it only include appliers. We keep the name for future extension.
// TODO: If it turns out this struct only has appliers in the final version, we will rename it to a more specific name, e.g., AppliersMap
struct RestoreSysInfo {
	std::map<UID, RestoreApplierInterface> appliers;

	RestoreSysInfo() = default;
	explicit RestoreSysInfo(const std::map<UID, RestoreApplierInterface> appliers) : appliers(appliers) {}

	template <class Ar>
	void serialize(Ar& ar) {
		serializer(ar, appliers);
	}
};

struct RestoreWorkerInterface {
	UID interfID;

	RequestStream<RestoreSimpleRequest> heartbeat;
	RequestStream<RestoreRecruitRoleRequest> recruitRole;
	RequestStream<RestoreSimpleRequest> terminateWorker;

	bool operator == (RestoreWorkerInterface const& r) const { return id() == r.id(); }
	bool operator != (RestoreWorkerInterface const& r) const { return id() != r.id(); }

	UID id() const { return interfID; } //cmd.getEndpoint().token;

	NetworkAddress address() const { return recruitRole.getEndpoint().addresses.address; }

	void initEndpoints() {
		heartbeat.getEndpoint( TaskClusterController );
		recruitRole.getEndpoint( TaskClusterController );// Q: Why do we need this? 
		terminateWorker.getEndpoint( TaskClusterController ); 

		interfID = g_random->randomUniqueID();
	}

	template <class Ar>
	void serialize( Ar& ar ) {
		serializer(ar, interfID, heartbeat, recruitRole, terminateWorker);
	}
};

struct RestoreRoleInterface {
	UID nodeID;
	RestoreRole role;

	RestoreRoleInterface() {
		role = RestoreRole::Invalid;
	}

	explicit RestoreRoleInterface(RestoreRoleInterface const& interf) : nodeID(interf.nodeID), role(interf.role) {};

	UID id() const { return nodeID; }

	std::string toString() {
		std::stringstream ss;
		ss << "Role:" << getRoleStr(role) << " interfID:" << nodeID.toString();
		return ss.str();
	}

	template <class Ar>
	void serialize( Ar& ar ) {
		serializer(ar, nodeID, role);
	}
};

struct RestoreLoaderInterface : RestoreRoleInterface {
	RequestStream<RestoreSimpleRequest> heartbeat;
	RequestStream<RestoreSysInfoRequest> updateRestoreSysInfo;
	RequestStream<RestoreSetApplierKeyRangeVectorRequest> setApplierKeyRangeVectorRequest;
	RequestStream<RestoreLoadFileRequest> loadFile;
	RequestStream<RestoreVersionBatchRequest> initVersionBatch;
	RequestStream<RestoreSimpleRequest> collectRestoreRoleInterfaces; // TODO: Change to collectRestoreRoleInterfaces
	RequestStream<RestoreVersionBatchRequest> finishRestore;

	bool operator == (RestoreWorkerInterface const& r) const { return id() == r.id(); }
	bool operator != (RestoreWorkerInterface const& r) const { return id() != r.id(); }

	RestoreLoaderInterface () {
		role = RestoreRole::Loader;
		nodeID = g_random->randomUniqueID();
	}

	NetworkAddress address() const { return heartbeat.getEndpoint().addresses.address; }

	void initEndpoints() {
		heartbeat.getEndpoint( TaskClusterController );
		updateRestoreSysInfo.getEndpoint( TaskClusterController );
		setApplierKeyRangeVectorRequest.getEndpoint( TaskClusterController ); 
		loadFile.getEndpoint( TaskClusterController ); 
		initVersionBatch.getEndpoint( TaskClusterController );
		collectRestoreRoleInterfaces.getEndpoint( TaskClusterController ); 
		finishRestore.getEndpoint( TaskClusterController ); 
	}

	template <class Ar>
	void serialize( Ar& ar ) {
		serializer(ar, * (RestoreRoleInterface*) this, heartbeat, updateRestoreSysInfo,
				setApplierKeyRangeVectorRequest, loadFile,
				initVersionBatch, collectRestoreRoleInterfaces, finishRestore);
	}
};


struct RestoreApplierInterface : RestoreRoleInterface {
	RequestStream<RestoreSimpleRequest> heartbeat;
	RequestStream<RestoreSendMutationVectorVersionedRequest> sendMutationVector;
	RequestStream<RestoreVersionBatchRequest> applyToDB;
	RequestStream<RestoreVersionBatchRequest> initVersionBatch;
	RequestStream<RestoreSimpleRequest> collectRestoreRoleInterfaces;
	RequestStream<RestoreVersionBatchRequest> finishRestore;


	bool operator == (RestoreWorkerInterface const& r) const { return id() == r.id(); }
	bool operator != (RestoreWorkerInterface const& r) const { return id() != r.id(); }

	RestoreApplierInterface() {
		role = RestoreRole::Applier;
		nodeID = g_random->randomUniqueID();
	}

	NetworkAddress address() const { return heartbeat.getEndpoint().addresses.address; }

	void initEndpoints() {
		heartbeat.getEndpoint( TaskClusterController );
		sendMutationVector.getEndpoint( TaskClusterController ); 
		applyToDB.getEndpoint( TaskClusterController ); 
		initVersionBatch.getEndpoint( TaskClusterController );
		collectRestoreRoleInterfaces.getEndpoint( TaskClusterController ); 
		finishRestore.getEndpoint( TaskClusterController ); 
	}

	template <class Ar>
	void serialize( Ar& ar ) {
		serializer(ar,  * (RestoreRoleInterface*) this, heartbeat, 
				sendMutationVector, applyToDB, initVersionBatch, collectRestoreRoleInterfaces, finishRestore);
	}

	std::string toString() {
		return nodeID.toString();
	}
};

// TODO: MX: It is probably better to specify the (beginVersion, endVersion] for each loadingParam. beginVersion (endVersion) is the version the applier is before (after) it receives the request.
struct LoadingParam {
	bool isRangeFile;
	Key url;
	Version prevVersion;
	Version endVersion;
	Version version;
	std::string filename;
	int64_t offset;
	int64_t length;
	int64_t blockSize;
	KeyRange restoreRange;
	Key addPrefix;
	Key removePrefix;
	Key mutationLogPrefix;

	// TODO: Compare all fields for loadingParam
	bool operator == ( const LoadingParam& r ) const { return isRangeFile == r.isRangeFile && filename == r.filename; }
	bool operator != ( const LoadingParam& r ) const { return isRangeFile != r.isRangeFile || filename != r.filename; }
	bool operator < ( const LoadingParam& r ) const {
		return (isRangeFile < r.isRangeFile) ||
			(isRangeFile == r.isRangeFile && filename < r.filename);
	}

	template <class Ar>
	void serialize(Ar& ar) {
		serializer(ar, isRangeFile, url, prevVersion, endVersion, version, filename, offset, length, blockSize, restoreRange, addPrefix, removePrefix, mutationLogPrefix);
	}

	std::string toString() {
		std::stringstream str;
		str << "isRangeFile:" << isRangeFile << "url:" << url.toString() << " prevVersion:" << prevVersion << " endVersion:" << endVersion << " version:" << version
			<<  " filename:" << filename  << " offset:" << offset << " length:" << length << " blockSize:" << blockSize
			<< " restoreRange:" << restoreRange.toString()
			<< " addPrefix:" << addPrefix.toString() << " removePrefix:" << removePrefix.toString();
		return str.str();
	}
};

struct RestoreRecruitRoleReply : TimedRequest {
	UID id;
	RestoreRole role;
	Optional<RestoreLoaderInterface> loader;
	Optional<RestoreApplierInterface> applier;

	RestoreRecruitRoleReply() = default;
	explicit RestoreRecruitRoleReply(UID id, RestoreRole role, RestoreLoaderInterface const& loader): id(id), role(role), loader(loader) {}
	explicit RestoreRecruitRoleReply(UID id, RestoreRole role, RestoreApplierInterface const& applier): id(id), role(role), applier(applier) {}

	template <class Ar> 
	void serialize( Ar& ar ) {
		serializer(ar, id, role, loader, applier);
	}

	std::string toString() {
		std::stringstream ss;
		ss << "roleInterf role:" << getRoleStr(role) << " replyID:" << id.toString();
		if (loader.present()) {
			ss << "loader:" <<  loader.get().toString();
		}
		if (applier.present()) {
			ss << "applier:" << applier.get().toString();
		}
			
		return ss.str();
	}
};

struct RestoreRecruitRoleRequest : TimedRequest {
	RestoreRole role;
	int nodeIndex; // Each role is a node

	ReplyPromise<RestoreRecruitRoleReply> reply;

	RestoreRecruitRoleRequest() :  role(RestoreRole::Invalid) {}
	explicit RestoreRecruitRoleRequest(RestoreRole role, int nodeIndex) : role(role), nodeIndex(nodeIndex){}

	template <class Ar> 
	void serialize( Ar& ar ) {
		serializer(ar, role, nodeIndex, reply);
	}

	std::string printable() {
		std::stringstream ss;
		ss <<  "RestoreRecruitRoleRequest Role:" << getRoleStr(role) << " NodeIndex:" << nodeIndex;
		return ss.str();
	}

	std::string toString() {
		return printable();
	}
};

struct RestoreSysInfoRequest : TimedRequest {
	RestoreSysInfo sysInfo;

	ReplyPromise<RestoreCommonReply> reply;

	RestoreSysInfoRequest() = default;
	explicit RestoreSysInfoRequest(RestoreSysInfo sysInfo) : sysInfo(sysInfo) {}

	template <class Ar>
	void serialize(Ar& ar) {
		serializer(ar, sysInfo, reply);
	}

	std::string toString() {
		std::stringstream ss;
		ss <<  "RestoreSysInfoRequest";
		return ss.str();
	}
};


// Sample_Range_File and Assign_Loader_Range_File, Assign_Loader_Log_File
struct RestoreLoadFileRequest : TimedRequest {
	LoadingParam param;

	ReplyPromise<RestoreCommonReply> reply;

	RestoreLoadFileRequest() = default;
	explicit RestoreLoadFileRequest(LoadingParam param) : param(param) {}

	template <class Ar> 
	void serialize( Ar& ar ) {
		serializer(ar, param, reply);
	}

	std::string toString() {
		std::stringstream ss;
		ss <<  "RestoreLoadFileRequest param:" << param.toString();
		return ss.str();
	}
};

struct RestoreSendMutationVectorVersionedRequest : TimedRequest {
	Version prevVersion, version; // version is the commitVersion of the mutation vector.
	bool isRangeFile;
	Standalone<VectorRef<MutationRef>> mutations; // All mutations are at version

	ReplyPromise<RestoreCommonReply> reply;

	RestoreSendMutationVectorVersionedRequest() = default;
	explicit RestoreSendMutationVectorVersionedRequest(Version prevVersion, Version version, bool isRangeFile, VectorRef<MutationRef> mutations) :
			 prevVersion(prevVersion), version(version), isRangeFile(isRangeFile), mutations(mutations) {}

	std::string toString() {
		std::stringstream ss;
		ss << "prevVersion:" << prevVersion << " version:" << version << " isRangeFile:" << isRangeFile << " mutations.size:" << mutations.size();
		return ss.str();
	}

	template <class Ar> 
	void serialize( Ar& ar ) {
		serializer(ar, prevVersion, version, isRangeFile, mutations, reply);
	}
};


struct RestoreVersionBatchRequest : TimedRequest {
	int batchID;

	ReplyPromise<RestoreCommonReply> reply;

	RestoreVersionBatchRequest() = default;
	explicit RestoreVersionBatchRequest(int batchID) : batchID(batchID) {}

	template <class Ar> 
	void serialize( Ar& ar ) {
		serializer(ar, batchID, reply);
	}

	std::string toString() {
		std::stringstream ss;
		ss << "RestoreVersionBatchRequest BatchID:"  <<  batchID;
		return ss.str();
	}
};

struct RestoreSetApplierKeyRangeVectorRequest : TimedRequest {
	std::map<Standalone<KeyRef>, UID> range2Applier;
	
	ReplyPromise<RestoreCommonReply> reply;

	RestoreSetApplierKeyRangeVectorRequest() = default;
	explicit RestoreSetApplierKeyRangeVectorRequest(std::map<Standalone<KeyRef>, UID> range2Applier) : range2Applier(range2Applier) {}

	template <class Ar> 
	void serialize( Ar& ar ) {
		serializer(ar, range2Applier, reply);
	}

	std::string toString() {
		std::stringstream ss;
		ss << "RestoreVersionBatchRequest range2ApplierSize:"  <<  range2Applier.size();
		return ss.str();
	}
};

struct RestoreRequest {
	//Database cx;
	int index;
	Key tagName;
	Key url;
	bool waitForComplete;
	Version targetVersion;
	bool verbose;
	KeyRange range;
	Key addPrefix;
	Key removePrefix;
	bool lockDB;
	UID randomUid;

	int testData;
	std::vector<int> restoreRequests;
	//Key restoreTag;

	ReplyPromise< struct RestoreReply > reply;

	RestoreRequest() : testData(0) {}
	explicit RestoreRequest(int testData) : testData(testData) {}
	explicit RestoreRequest(int testData, std::vector<int> &restoreRequests) : testData(testData), restoreRequests(restoreRequests) {}

	explicit RestoreRequest(const int index, const Key &tagName, const Key &url, bool waitForComplete, Version targetVersion, bool verbose,
							const KeyRange &range, const Key &addPrefix, const Key &removePrefix, bool lockDB,
							const UID &randomUid) : index(index), tagName(tagName), url(url), waitForComplete(waitForComplete),
													targetVersion(targetVersion), verbose(verbose), range(range),
													addPrefix(addPrefix), removePrefix(removePrefix), lockDB(lockDB),
													randomUid(randomUid) {}

	template <class Ar>
	void serialize(Ar& ar) {
		serializer(ar, index , tagName , url ,  waitForComplete , targetVersion , verbose , range , addPrefix , removePrefix , lockDB , randomUid ,
		testData , restoreRequests , reply);
	}

	//Q: Should I convert this toString() to a function to dump RestoreRequest to TraceEvent?
	std::string toString() const {
		std::stringstream ss;
		ss <<  "index:" << std::to_string(index) << " tagName:" << tagName.contents().toString() << " url:" << url.contents().toString()
			   << " waitForComplete:" << std::to_string(waitForComplete) << " targetVersion:" << std::to_string(targetVersion)
			   << " verbose:" << std::to_string(verbose) << " range:" << range.toString() << " addPrefix:" << addPrefix.contents().toString()
			   << " removePrefix:" << removePrefix.contents().toString() << " lockDB:" << std::to_string(lockDB) << " randomUid:" << randomUid.toString();
		return ss.str();
	}
};


struct RestoreReply {
	int replyData;

	RestoreReply() : replyData(0) {}
	explicit RestoreReply(int replyData) : replyData(replyData) {}

	template <class Ar>
	void serialize(Ar& ar) {
		serializer(ar, replyData);
	}
};

std::string getRoleStr(RestoreRole role);

////--- Interface functions
Future<Void> _restoreWorker(Database const& cx, LocalityData const& locality);
Future<Void> restoreWorker(Reference<ClusterConnectionFile> const& ccf, LocalityData const& locality);


// Send each request in requests via channel of the request's interface.
// Do not expect a meaningful reply
// The UID in a request is the UID of the interface to handle the request
ACTOR template <class Interface, class Request>
//Future< REPLY_TYPE(Request) > 
Future<Void> sendBatchRequests(
	RequestStream<Request> Interface::* channel,
	std::map<UID, Interface> interfaces,
	std::vector<std::pair<UID, Request>> requests) {
	
	if ( requests.empty() ) {
		return Void();
	}

	loop{
		try {		
			std::vector<Future<REPLY_TYPE(Request)>> cmdReplies;
			for(auto& request : requests) {
				RequestStream<Request> const* stream = & (interfaces[request.first].*channel);
				cmdReplies.push_back( stream->getReply(request.second) );
			}

			// Alex: Unless you want to do some action when it timeout multiple times, you should use timout. Otherwise, getReply will automatically keep retrying for you.
			std::vector<REPLY_TYPE(Request)> reps = wait( timeoutError(getAll(cmdReplies), SERVER_KNOBS->FASTRESTORE_FAILURE_TIMEOUT) ); //tryGetReply. Use GetReply. // Alex: you probably do NOT need the timeoutError. 
			//wait( waitForAll(cmdReplies) ); //tryGetReply. Use GetReply. // Alex: you probably do NOT need the timeoutError. 
			break;
		} catch (Error &e) {
			if ( e.code() == error_code_operation_cancelled ) break;
			fprintf(stdout, "sendBatchRequests Error code:%d, error message:%s\n", e.code(), e.what());
			for (auto& request : requests ) {
				TraceEvent(SevWarn, "FastRestore").detail("SendBatchRequests", requests.size())
					.detail("RequestID", request.first).detail("Request", request.second.toString());
			}
		}
	}

	return Void();
} 

// Similar to sendBatchRequests except that the caller expect to process the reply.
// This actor can be combined with sendBatchRequests(...)
ACTOR template <class Interface, class Request>
//Future< REPLY_TYPE(Request) > 
Future<Void> getBatchReplies(
	RequestStream<Request> Interface::* channel,
	std::map<UID, Interface> interfaces,
	std::map<UID, Request> requests,
	std::vector<REPLY_TYPE(Request)>* replies) {

	if ( requests.empty() ) {
		return Void();
	}

	loop{
		try {		
			std::vector<Future<REPLY_TYPE(Request)>> cmdReplies;
			for(auto& request : requests) {
				RequestStream<Request> const* stream = & (interfaces[request.first].*channel);
				cmdReplies.push_back( stream->getReply(request.second) );
			}

			// Alex: Unless you want to do some action when it timeout multiple times, you should use timout. Otherwise, getReply will automatically keep retrying for you.
			std::vector<REPLY_TYPE(Request)> reps = wait( timeoutError(getAll(cmdReplies), SERVER_KNOBS->FASTRESTORE_FAILURE_TIMEOUT) ); //tryGetReply. Use GetReply. // Alex: you probably do NOT need the timeoutError. 
			*replies = reps;
			break;
		} catch (Error &e) {
			if ( e.code() == error_code_operation_cancelled ) break;
			fprintf(stdout, "getBatchReplies Error code:%d, error message:%s\n", e.code(), e.what());
		}
	}

	return Void();
} 


#include "flow/unactorcompiler.h"
#endif