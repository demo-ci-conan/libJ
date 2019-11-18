def artifactory_name = "Artifactory Docker"
def artifactory_repo = "conan-local"
def docker_runs = [:]  // [id] = [docker_image, profile]

docker_runs["conanio-gcc8"] = ["conanio/gcc8", "linux_gcc_8_x86_64"]
docker_runs["conanio-gcc7"] = ["conanio/gcc7", "linux_gcc_7_x86_64"]
// Conan arm docker images are not updated so we can not use conan_build_info --v
// docker_runs["conanio-gcc8-armv7hf"] = ["conanio/gcc8-armv7hf", "linux_gcc_8_armv7hf"]
// docker_runs["conanio-gcc7-armv7hf"] = ["conanio/gcc7-armv7hf", "linux_gcc_7_armv7hf"]

// temporary
docker_runs["conanio-gcc8_temp"] = ["conanio/gcc8", "conanio-gcc8"]	
docker_runs["conanio-gcc7_temp"] = ["conanio/gcc7", "conanio-gcc7"]

def organization = "demo-ci-conan"
def user_channel = "demo/testing"
def config_url = "https://github.com/demo-ci-conan/settings.git"
def projects = ["App1/0.0@${user_channel}", "App2/0.0@${user_channel}", ]  // TODO: Get list dinamically

def get_stages(id, docker_image, artifactory_name, artifactory_repo, profile, user_channel, config_url) {
    return {
        node {
            docker.image(docker_image).inside("--net=docker_jenkins_artifactory") {
                sh "pip install git+git://github.com/czoido/conan.git@hackaton_branch"
                def scmVars = checkout scm
                def repo_name = scmVars.GIT_URL.tokenize('/')[3].split("\\.")[0]
                withEnv(["CONAN_USER_HOME=${env.WORKSPACE}/conan_cache"]) {
                    def server = Artifactory.server artifactory_name
                    def client = Artifactory.newConanClient(userHome: "${env.WORKSPACE}/conan_cache".toString())
                    def remoteName = "artifactory-local"
                    def lockfile = "${id}.lock"
                    try {

                        def buildInfo = Artifactory.newBuildInfo()

                        stage("Start build info") {
                            String start_build_info = "conan_build_info --v2 start \"${buildInfo.getName()}\" ${buildInfo.getNumber()}"
                            sh start_build_info
                        }

                        client.run(command: "config install ${config_url}".toString())
                        client.remote.add server: server, repo: artifactory_repo, remoteName: remoteName, force: true

                        stage("${id}") {
                            echo 'Running in ${docker_image}'
                        }

                        stage("Get dependencies and create app") {
                            String arguments = "--profile ${profile} --lockfile=${lockfile}"
                            client.run(command: "graph lock . ${arguments}".toString())
                            client.run(command: "create . ${user_channel} ${arguments} --build ${repo_name} --ignore-dirty".toString())
                            sh "cat ${lockfile}"
                        }

                        stage("Calculate full reference") {
                            if (id=="conanio-gcc8") {// TODO fix this
                                name = sh (script: "conan inspect . --raw name", returnStdout: true).trim()
                                version = sh (script: "conan inspect . --raw version", returnStdout: true).trim()
                                def search_output = "search_output.json"
                                sh "conan search ${name}/${version}@${user_channel} --revisions --raw --json=${search_output}"
                                stash name: "full_reference", includes: search_output
                                sh "cat search_output.json"
                            }
                        }

                        stage("Upload packages") {
                            String uploadCommand = "upload ${repo_name}* --all -r ${remoteName} --confirm  --force"
                            client.run(command: uploadCommand)
                        }

                        stage("Create build info") {
                            def buildInfoFilename = "${id}.json"
                            client.run(command: "search *".toString())
                            // TODO: manage credentials
                            String create_build_info = "conan_build_info --v2 create --lockfile ${lockfile} --user admin --password password ${buildInfoFilename}"
                            sh create_build_info
                            echo "Stash '${id}' -> '${buildInfoFilename}'"
                            stash name: id, includes: "${buildInfoFilename}"
                        }
                    }
                    finally {
                        deleteDir()
                    }
                }
            }
        }
    }
}

def stages = [:]
docker_runs.each { id, values ->
    stages[id] = get_stages(id, values[0], artifactory_name, artifactory_repo, values[1], user_channel, config_url)
}

node {
    try {
        stage("Build + upload") {
            withEnv(["CONAN_HOOK_ERROR_LEVEL=40"]) {
                parallel stages
            }
        }
        stage("Retrieve and publish build info") {
            docker.image("conanio/gcc8").inside("--net=docker_jenkins_artifactory") {
                sh "pip install git+git://github.com/czoido/conan.git@hackaton_branch"
                def server = Artifactory.server artifactory_name
                def last_info = ""
                docker_runs.each { id, values ->
                    unstash id
                    sh "cat ${id}.json"
                    if (last_info != "") {
                        sh "conan_build_info --v2 update ${id}.json ${last_info} --output-file mergedbuildinfo.json"
                        sh "cat mergedbuildinfo.json"
                    }
                    last_info = "${id}.json"
                }
                // TODO: configure credentials properly
                String publish_build_info = "conan_build_info --v2 publish --url ${server.url} --user admin --password password mergedbuildinfo.json"
                sh publish_build_info
            }
        }

        stage("Launch job-graph") {
            def scmVars = checkout scm

            stage("Trigger dependents jobs") {
                unstash "full_reference"
                sh "cat search_output.json"
                
                def props = readJSON file: "search_output.json"
                def revision = props[0]['revision']
                def reference = "${name}/${version}@${user_channel}#${revision}"
                echo "Full reference: '${reference}'"

                def repository = scmVars.GIT_URL.tokenize('/')[3].split("\\.")[0]
                def sha1 = scmVars.GIT_COMMIT
                projects.each {project_id -> 
                    def json = """{"parameter": [{"name": "reference", "value": "${reference}"}, \
                                                    {"name": "project_id", "value": "${project_id}"}, \
                                                    {"name": "organization", "value": "${organization}"}, \
                                                    {"name": "repository", "value": "${repository}"}, \
                                                    {"name": "sha1", "value": "${sha1}"} \
                                                    ]}"""
                    withCredentials([usernamePassword(credentialsId: 'job-graph', passwordVariable: 'pass', usernameVariable: 'user')]) {
                        // TODO: FIXME: user pass from credentials 
                        def jenkins_user_token = "admin:1180edb4037ce3fb2dae7260d2cf4ddcb2"
                        if (env.JENKINS_USER_TOKEN) {
                            jenkins_user_token = "${env.JENKINS_USER_TOKEN}"
                        }
                        def url = "${env.JENKINS_URL}job/test_project/build"
                        sh "curl -u ${jenkins_user_token} -v POST ${url} --data-urlencode json='${json}'"
                    }                            
                }
            }
        }
    }
    finally {
        deleteDir()
    }
}

