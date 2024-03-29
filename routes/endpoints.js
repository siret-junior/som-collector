
/* This file is part of SOMHunter.
 *
 * Copyright (C) 2020 František Mejzlík <frankmejzlik@gmail.com>
 *                    Mirek Kratochvil <exa.exa@gmail.com>
 *                    Patrik Veselý <prtrikvesely@gmail.com>
 *
 * SOMHunter is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 2 of the License, or (at your option)
 * any later version.
 *
 * SOMHunter is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * SOMHunter. If not, see <https://www.gnu.org/licenses/>.
 */
"use strict";

const fs = require("fs");
const path = require("path");

const SessionState = require("./common/SessionState");

exports.getFrameDetailData = function (req, res) {
  const sess = req.session;

  const frameId = Number(req.query.frameId);

  let frameData = {};
  // -------------------------------
  // Call the core
  frameData = global.core.getDisplay(req.session.user, global.cfg.framesPathPrefix, "detail", null, frameId);
  // -------------------------------

  res.status(200).jsonp(frameData);
};

exports.getPreviousScreen = function (req, res) {
  const sess = req.session;

  const frameId = 0;

  let frameData = {};
  // -------------------------------
  // Call the core
  frameData = global.core.getPreviousDisplay(req.session.user, global.cfg.framesPathPrefix, "previous", null, frameId);
  // -------------------------------
  
  res.status(200).jsonp({ viewData: frameData });
}
exports.getSomScreen = function (req, res) {
  const sess = req.session;

  let frameData = {};

  if (!global.core.isSomReady(req.session.user)) {
    res.status(200).jsonp({ viewData: null, error: { message: "SOM not yet ready." } });
    return;
  }

  // -------------------------------
  // Call the core
  frameData = global.core.getDisplay(req.session.user, global.cfg.framesPathPrefix, "som");
  // -------------------------------

  SessionState.switchScreenTo(sess.state, "som", frameData.frames, 0);

  let viewData = {};
  viewData.somhunter = SessionState.getSomhunterUiState(sess.state);

  res.status(200).jsonp({ viewData: viewData });
};

exports.getTopScreen = function (req, res) {
  const sess = req.session;

  global.logger.log("info", req.query)
  let type = "topn";
  if (req.query && req.query.type)
    type = req.query.type;

  let pageId = 0;
  if (req.query && req.query.pageId)
    pageId = Number(req.query.pageId);

  let frameId = 0;
  if (req.query && req.query.frameId)
    frameId = Number(req.query.frameId);

  let frames = [];
  // -------------------------------
  // Call the core
  const displayFrames = global.core.getDisplay(req.session.user, global.cfg.framesPathPrefix, type, pageId, frameId);
  frames = displayFrames.frames;
  // -------------------------------

  SessionState.switchScreenTo(sess.state, type, frames, frameId);

  let viewData = {};
  viewData.somhunter = SessionState.getSomhunterUiState(sess.state);

  res.status(200).jsonp({ viewData: viewData });
};

exports.rescore = function (req, res) {
  const sess = req.session;

  const body = req.body;
  const q0 = body.q0;
  const q1 = "";

  let textQuery = q0;

  // Append temporal query
  if (q1 != "") {
    textQuery += " >> ";
    textQuery += q1;
  }

  SessionState.setTextQueries(sess.state, q0, q1);

  const likes = SessionState.getLikes(sess.state);
  const unlikes = SessionState.getUnlikes(sess.state);

  // -------------------------------
  // Call the core
  global.core.addLikes(req.session.user, likes);
  global.core.removeLikes(req.session.user, unlikes);
  const state = global.core.rescore(req.session.user, textQuery);
  // -------------------------------

  // Reset likes
  SessionState.resetLikes(sess.state);
  SessionState.resetUnlikes(sess.state);

  let viewData = {};
  viewData.somhunter = SessionState.getSomhunterUiState(sess.state);
  viewData.somhunter.display_available = state.nextDisplay;
  viewData.somhunter.reformulations = state.reformulations;
  viewData.somhunter.feedbacks = state.feedbacks;

  res.status(200).jsonp({ viewData: viewData });
};

exports.submitFrame = function (req, res) {
  const sess = req.session;

  const body = req.body;
  const submittedFrameId = body.frameId;

  // -------------------------------
  // Call the core
  const sub = global.core.submitToServer(req.session.user, submittedFrameId);
  // -------------------------------

  res.status(200).jsonp(sub);
};

exports.getLevelInfo = function (req, res) {
  const sess = req.session;

  // -------------------------------
  // Call the core
  const sub = global.core.getLevelInfo(req.session.user);
  // -------------------------------

  res.status(200).jsonp(sub);
};

exports.reportIssue = function (req, res) {

  // -------------------------------
  // Call the core
  global.core.reportIssue(req.session.user);
  // -------------------------------

  res.status(200).jsonp({});
};

exports.getAutocompleteResults = function (req, res) {
  const sess = req.session;

  const prefix = req.query.queryValue;

  // -------------------------------
  // Call the core
  const acKeywords = global.core.autocompleteKeywords(
    req.session.user, 
    global.cfg.framesPathPrefix,
    prefix,
    global.cfg.autocompleteResCount
  );

  // Send response
  res.status(200).jsonp(acKeywords);
};

exports.getTarget = function (req, res) {
  const sess = req.session;

  // -------------------------------
  // Call the core
  const target = global.core.getTarget(req.session.user);
  
  global.logger.log("info", target)

  // Send response
  var resVal = global.cfg.framesPathPrefix + target.targetPath;
  if (target.targetPath == "nothing.jpg") {
    resVal = target.targetPath;
  }

  res.status(200).jsonp(resVal);
};

exports.resetSearchSession = function (req, res) {
  const sess = req.session;

  // -------------------------------
  // Call the core
  const avail_disp = global.core.resetAll(req.session.user);
  // -------------------------------

  SessionState.resetSearchSession(sess.state);

  let viewData = {};
  viewData.somhunter = SessionState.getSomhunterUiState(sess.state);
  viewData.somhunter.display_available = avail_disp;

  res.status(200).jsonp({ viewData: viewData });
};

exports.likeFrame = function (req, res) {
  const sess = req.session;

  const body = req.body;
  const frameId = body.frameId;

  // Handle and check the validity
  const succ = SessionState.likeFrame(sess.state, frameId);

  res.status(200).jsonp({ isLiked: succ });
};

exports.unlikeFrame = function (req, res) {
  const sess = req.session;

  const body = req.body;
  const frameId = body.frameId;

  // Handle and check the validity
  const succ = SessionState.unlikeFrame(sess.state, frameId);

  res.status(200).jsonp({ isLiked: false });
};
