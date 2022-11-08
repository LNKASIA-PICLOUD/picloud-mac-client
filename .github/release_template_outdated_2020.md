<!--
This is the template for new release issues.
TODO: split off a patch release template, so that this one is clearly only for major/minor releases.
-->

Open an issue called 'Release x.x.x' in client repository and copy below text into a task and tick the items:
<hr>

Major/Minor release template. Enter here, when we have three estimated dates:

* Date of feature freeze
* Date of QA start
* Date of final

## Reason

* Needed for branded client X.x.x release (link to branded release ticket)

### Prerequisites for Beta release:

* [ ] Check Sprint Board that no remaining ``to do`` issues left - @mstingl
* [ ] Dev: Internally announce feature freeze - @TheOneRing
* [ ] Dev: Make sure the previous version branch is merged into the current master branch e.g. 2.6 into master "one flow" - @TheOneRing @fmoc
* [ ] Dev: Check [dependencies](https://confluence.owncloud.com/display/OG/Dependencies) for updates - @TheOneRing @fmoc
* [ ] Dev: Update [ChangeLog](https://confluence.owncloud.com/display/OG/ChangeLog) - @TheOneRing @fmoc
* [ ] Dev: Update branch in translation https://github.com/owncloud/client/actions/workflows/translate.yml - @TheOneRing @fmoc
* [ ] Dev: Create new `X.x` version branch - @TheOneRing @fmoc
* [ ] Adjust branch of Cron Job `nightly-2-x` to the next release branch  @individual-it
* [ ] QA: Review list of [supported platforms](https://confluence.owncloud.com/display/OG/Supported+Platforms) -  @HanaGemela @fmoc @TheOneRing @mstingl
* [ ] QA: Update [documentation](https://confluence.owncloud.com/display/OG/Documentation) - @HanaGemela
* [ ] QA: Inform the docu team on rocketchat ``#documentation-internal`` about an upcoming major or minor release - @HanaGemela
* [ ] QA: Check the translations coming from https://github.com/owncloud/client/actions/workflows/translate.yml
* [ ] QA: Make sure [squish tests](https://confluence.owncloud.com/display/OG/Squish+Testing#SquishTesting-Prerequisite) are running successfully on X.x branch and on nightly builds for the current release, e.g. ``nightly-2-10`` 
* [ ] QA: Update Test Plans - @HanaGemela 

### Build Beta:

* [ ] Dev: Bump [`VERSION.cmake`](https://handbook.owncloud.com/release_processes/client/branch.html#version-cmake) - @TheOneRing 
* [ ] Dev: Run calens on the work branch - @TheOneRing @fmoc
* [ ] Dev: Edit and run release script - @TheOneRing @fmoc
* [ ] DEV: Tag Beta and build [builds](https://confluence.owncloud.com/display/OG/Build+and+Tags#BuildandTags-Sprintbuild) for theme 'ownCloud' and 'testpilotcloud' (includes ChangeLog for the tag on https://github.com/owncloud/client/releases/)
* [ ] Dev: Create github release - @TheOneRing @fmoc
* [ ] Dev: Ping marketing - @TheOneRing @fmoc
* [ ] DEV: Prepare the update server for new version (AppImages included)
* [ ] DEV: Provide 'testpilotcloud' on **Beta** update channel
* [ ] Dev: Start running automated tests on the dailies - @TheOneRing 
* [ ] QA: Adjust [Linux Templates](https://handbook.owncloud.com/release_processes/client/branch.html#linux-templates) - @HanaGemela @jnweiger
* [ ] QA: Adjust [ownBrander](https://handbook.owncloud.com/release_processes/client/branch.html#ownbrander) - @HanaGemela @jnweiger
* [ ] QA: Adjust [drone](https://handbook.owncloud.com/release_processes/client/branch.html#drone) - @HanaGemela @jnweiger
* [ ] TODO (possibly redundant): QA: Adjust [translation jobs](https://handbook.owncloud.com/release_processes/client/branch.html#translations) - @HanaGemela @jnweiger
* [ ] QA: Ping ``#documentation-internal``: Changelog is ready. (open issues in ``docs-client-desktop`` repo for already known doc-relevant items and mark them accordingly, e.g. backport to 2.X.x necessary)
* [ ] Beta/RC [Communication](https://confluence.owncloud.com/display/OG/Marketing+and+Communication)
   * [ ] Website links for beta (needed for the following posts)
   * [ ] Central post https://central.owncloud.org/tags/c/news/desktop
   * [ ] All other stakeholders
* [ ] Announce the new branch to community and advertise dailies for public testing - marketing
* [ ] QA: [Antivirus scan](https://confluence.owncloud.com/display/OG/Virus+Scanning) - @HanaGemela @jnweiger 
* [ ] TODO (DEV or QA): Upload linux gpg keys to key server [key_server_upload](https://gitea.owncloud.services/client/linux-docker-install/src/branch/master/key_server_upload.sh)
* [ ] QA: Check Crash reporter is available in Beta (WIN/Mac/Linux Appimage: start 'owncloud --debug' on cmd line, system tray right click menu: 'Crash now - qt fatal' -> report window not empty, sending the report works)

### For All Sprint Builds:

* [ ] Add latest updates to Changelog in the client source repository
* [ ] Branch off a release branch called VERSION-rcX or VERSION-betaX  (without v, v is for tags)
* [ ] Edit ```VERSION.cmake``` to set the suffix to beta1, beta2 etc in the release branch
* [ ] (TODO: move to patch-release checklist) Make sure to increase the version number of the branched of release, e.g. if you release 2.3.2 then you should change VERSION.cmake in 2.3 to 2.3.3 since that branch now will be 2.3.3
* [ ] Create [builds](https://handbook.owncloud.com/release_processes/client/build.html#sprint-build) for theme 'ownCloud' and 'testpilotcloud'  @jnweiger @hvonreth
* [ ] Check if *tar.xz.asc files are there. If not follow the [instructions](https://github.com/owncloud/enterprise/wiki/Desktop-Signing-Knowledge)
* [ ] Run [the smoke test](https://handbook.owncloud.com/release_processes/client/smoke_test.html)
* [ ] Linux: Run [test](https://gitea.owncloud.services/client/linux-docker-install/src/branch/master/RUN.sh) with repo=https://download.opensuse.org/repositories/isv:/ownCloud:/desktop:/testing
* [ ] Linux: add/remove [build targets](https://handbook.owncloud.com/release_processes/client/supported_platforms.html) @hvonreth @jnweiger
* [ ] Create a [signed tag](https://github.com/owncloud/enterprise/wiki/Desktop-Signing-Knowledge) using ```git tag -u E94E7B37 tagname```  @guruz @jnweiger (TODO: is this still needed?)
* [ ] Update the wordpress content at owncloud.org/download @florian - marketing
* [ ] Inform packagers: @dragotin (openSUSE) - marketing
* [ ] Announce on [central](https://central.owncloud.org) (copy old announcement, link to changelog, download links etc) TODO: itemize what goes into the announcement: deprecation warnings. ... - marketing
* [ ] Inform community mailinglists devel@owncloud.org and testpilots@owncloud.org (make sure to mention it is an rc). Link to the central post so discussion happens there. - marketing
* [ ] Check crash reporter after some days  @guruz @hvonreth
* [ ] Update unstable channel in the owncloud hosted auto updater. Instructions [here](https://github.com/owncloud/enterprise/blob/master/client_update_checker/README.md#deploy) and [here](https://handbook.owncloud.com/release_processes/client/desktop.html#update-the-updater) @hgemela @jnweiger
* [ ] QA: Run [automated tests](https://confluence.owncloud.com/display/OG/Automated+Tests) (includes [Smoke test](https://confluence.owncloud.com/display/OG/Manual+Tests#ManualTests-DEVSmokeTest))
* [ ] QA: All Linux platform install and gpg test ssh://git@gitea.owncloud.services:2222/client/linux-docker-install.git
  * [ ] manually deploy a linux download repo, or use a download repo from https://download.owncloud.com/desktop/ownCloud/testing/
  * [ ] in defs.sh edit `repo=` and update `platform_docker_images=`
  * [ ] RUN.sh -> paste the log/test_YYYYMMDD_hhmm.log file into the client release ticket.
* [ ] QA: Create the testplan according to release type (patch release: add tests in a comment, for major/minor release: create a separate ticket), see [Testplan Templates](https://confluence.owncloud.com/display/OG/Desktop+Client+Release+Process) - add the link here
* [ ] QA: Add the __Changelog Testing__ as a comment (to this ticket or the testplan), for changelog issues see [Client Releases](https://github.com/owncloud/client/releases/) - add the link here
* [ ] QA: If required: create a separate test plan ticket for Windows VFS testing from [VFS Template](https://github.com/owncloud/QA/blob/master/Desktop/Test_Plan_VFS.md) - add the link here


### One Week Before the Final Release (Skip this section for patch releases):

* [ ] Communicate the release schedule on rocket-chat #release-coordination and mailinglist release-coordination@owncloud.com. Give a high level overview of the upcoming new features, changes etc.
* [ ] Ensure marketing is aware (marketing@owncloud.com) and prepared for the release (social, .com website, cust.communications) - 1 week before minor, 2 weeks before major (minor/major is about impact)
* [ ] Inform Achim (ageissel@owncloud.com) and GCX that the next version will be in 1 week (gcx@owncloud.com) - marketing

### One Day Before the Final Release:
* [ ] Check [crash reporter](https://handbook.owncloud.com/release_processes/client/desktop.html#crash-reporter) for bad crashes of this RC (same crash happening to many users) @guruz @hvonreth
* [ ] Check the translations coming from transifex: All synchronized? TODO: (20181109jw: where? how?)
* [ ] Review drone results: `make test` TODO: Mac, [Lin](https://drone.owncloud.services/client/build-linux), Win? 
* [ ] DEV: [Smash box test](https://drone.owncloud.com/owncloud/smashbox-testing) Make sure tests run on latest version - @TheOneRing @fmoc
* [ ] Inform product management and marketing and #general channel in rocker chat that we are 1 day out

### On Release Day (for the Final Release):
For major, minor, and patch releases, but skip this section for ALPHA/BETA

* [ ] Add last updates to Changelog in the client source repository @hvonreth @hgemela
* [ ] Branch off a release branch once QA is done, tag with a release vA.B.C (with v, as v is for tags) @hvonreth @guruz
* [ ] Edit ```VERSION.cmake``` hange suffix from 'git' or 'rc' to empty string "". Commit the result to the release branch only @hvonreth @guruz
* [ ] Create [builds](https://handbook.owncloud.com/release_processes/client/build.html#final-build) for theme 'ownCloud' and 'testpilotcloud'  @jnweiger @hvonreth
* [ ] Check if *tar.xz.asc files are [here](https://download.owncloud.com/desktop/testing). If not follow the [instructions](https://github.com/owncloud/enterprise/wiki/Desktop-Signing-Knowledge)
* [ ] Branch isv:ownCloud:desktop to isv:ownCloud:desktop:client-X.Y.Z using [obs_integration](https://github.com/owncloud/administration/blob/master/jenkins/obs_integration/) the Linux packages will always land in the :testing repository - @jnweiger
  ```obs-deepcopy-prj.sh isv:ownCloud:desktop isv:ownCloud:desktop:client-2.5.1```
* [ ] Run [the smoke test](https://handbook.owncloud.com/release_processes/client/smoke_test.html)
* [ ] Linux: Run [test](https://gitea.owncloud.services/client/linux-docker-install/src/branch/master/RUN.sh) repo=https://download.opensuse.org/repositories/isv:/ownCloud:/desktop:/testing - @jnweiger
* [ ] Win/Mac Copy builds from ```testing``` to ```stable``` on download.owncloud.com, double check the download links. (make sure the .asc is there too.
* [ ] Linux: also copy the *.linux-repo.html files from ```testing``` to ```stable``` **and** edit away the `:testing` strings.
* [ ] Linux: disable publishing on project isv:ownCloud:desktop
* [ ] Linux: copy from testing to released in OBS:
  ```obs-deepcopy-prj.sh isv:ownCloud:desktop:testing isv:ownCloud:desktop```
  ```obs-deepcopy-prj.sh isv:ownCloud:testpilot:testing isv:ownCloud:testpilot```
* [ ] Linux: Re-enable OBS publishing on the project after official release date and if all distros build (check for accidentially disabled packages too) 
* [ ] Test all advertised download links to have the expected version
* [ ] Linux: Wait until everything is built and published, then disable publishing on project isv:ownCloud:desktop
* [ ] Create git [signed](https://github.com/owncloud/enterprise/wiki/Desktop-Signing-Knowledge) tag in client repository using ```git tag -u E94E7B37 tagname``` 
* [ ] Increment version number in daily builds. Special case: after the last release in a branch, jump forward to the 'next release branch'... That may mean, this is nightly is the same as edge then.
* [ ] Create same tag for MSI code - @dschmidt 
* [ ] Create same tag for Windows toolchain - @dschmidt 
* [ ] Create same tag (actually a symlink) for macOS toolchain - @dschmidt 
* [ ] Create a (draft) release [here](https://github.com/owncloud/client/releases)
* [ ] 1h later check [changelog on website](https://owncloud.org/changelog/desktop-client/) -> it pulls from the master branch ChangeLog file hourly.  - @jnweiger
* [ ] Update [org website](https://owncloud.org/download/#owncloud-desktop-client) -> Download ownCloud -> click open 'Desktop Client', edit win/mac/lin, each all three tabs "Production", "Technical Preview" [disabled], "Test pilot" enabled, edit the links.
* [ ] Add the previous release to [older version](https://owncloud.org/download/older-versions/) @jnweiger + fwittek
* [ ] Ping marketing to do their [actions](https://handbook.owncloud.com/release_processes/client/marketing.html)
* [ ] Take pride and celebrate!
* [ ] Tell GCX to increment the minimum supported version for enterprise customers - @mstingl
* [ ] Check if [minimum.supported.desktop.version](https://github.com/owncloud/core/blob/master/config/config.sample.php#L1152) needs to be updated in server 
 * [ ] Ensure that the [client release template](https://github.com/owncloud/client/edit/notes-from-the-etherpad/.github/release_template.md) is up to date
* [ ] After OBS built everything, disable publishing in OBS to prevent that accidential rebuilds hit the end users.- @jnweiger

## A few days after the release (for final release)

* [ ] DEV: Check the [crash reporter](https://confluence.owncloud.com/display/OG/Online+Updater%2C+Crash+reporter%2C+Transifex#OnlineUpdater,Crashreporter,Transifex-CrashReporter) for bad/frequent crashes
* [ ] Update the owncloud hosted auto [updater](https://github.com/owncloud/enterprise/blob/master/client_update_checker/README.md#deploy)  
