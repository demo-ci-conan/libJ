
def artifactory_name = "Artifactory Docker"
def artifactory_repo = "conan-local"
def docker_runs = [:]  // [id] = [docker_image, profile]
docker_runs["conanio-gcc8"] = ["conanio/gcc8", "conanio-gcc8"]
docker_runs["conanio-gcc7"] = ["conanio/gcc7", "conanio-gcc7"]

def organization = "demo-ci-conan"
def user_channel = "demo/testing"
def config_url = "https://github.com/demo-ci-conan/settings.git"

def get_stages(id, docker_image, artifactory_name, artifactory_repo, profile, user_channel, config_url) {
    return {
        node {
            docker.image(docker_image).inside("--net=docker_jenkins_artifactory") {
                def server = Artifactory.server artifactory_name
                def client = Artifactory.newConanClient()
                def remoteName = "artifactory-local"
                def lockfile = "${id}.lock"

                try {
                    client.run(command: "config install ${config_url}".toString())
                    client.run(command: "config install -sf hooks -tf hooks https://github.com/conan-io/hooks.git")
                    client.remote.add server: server, repo: artifactory_repo, remoteName: remoteName, force: true

                    stage("${id}") {
                        echo 'Running in ${docker_image}'
                    }

                    def scmVars = checkout scm
                    def repo_name = scmVars.GIT_URL.tokenize('/')[3].split("\\.")[0]
                    
                    stage("Get dependencies and create app") {
                        String arguments = "--profile ${profile} --lockfile=${lockfile}"
                        client.run(command: "graph lock . ${arguments}".toString())
                        client.run(command: "create --ignore-dirty . ${user_channel} ${arguments}".toString())
                        sh "cat ${lockfile}"
                    }

                    stage("Upload packages") {
                        String uploadCommand = "upload ${repo_name}* --all -r ${remoteName} --confirm"
                        client.run(command: uploadCommand)
                    }

                    /*
                    stage("Compute build info") {
                        def buildInfo = Artifactory.newBuildInfo()
                        String artifactory_credentials = "http://artifactory:8081/artifactory,admin,password"
                        def buildInfoFilename = "${id}.json"

                        // Install helper script (WIP)
                        git url: 'https://gist.github.com/a39acad525fd3e7e5315b2fa0bc70b6f.git'
                        sh 'pip install rtpy'

                        String python_command = "python lockfile_buildinfo.py --remotes=${artifactory_credentials}"
                        python_command += " --build-number=${buildInfo.getNumber()} --build-name=\"${buildInfo.getName()}\""
                        python_command += " --multi-module"
                        python_command += " --output-file=${buildInfoFilename} ${lockfile}"
                        sh python_command

                        echo "Stash '${id}' -> '${buildInfoFilename}'"
                        stash name: id, includes: "${buildInfoFilename}"
                    }
                    */
                }
                finally {
                    deleteDir()
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

        /*
        stage("Retrieve build info") {
            docker.image("conanio/gcc8").inside("--net=docker_jenkins_artifactory") {
                def buildInfo = Artifactory.newBuildInfo()
                String artifactory_credentials = "http://artifactory:8081/artifactory,admin,password"
                def buildInfoFilename = "buildinfo.json"
                
                // Install helper script (WIP)
                git url: 'https://gist.github.com/a39acad525fd3e7e5315b2fa0bc70b6f.git'
                sh 'pip install rtpy'

                // Merge Build Info from nodes
                String merge_bi_command = "python merge_buildinfo.py --output-file ${buildInfoFilename}"
                docker_runs.each { id, values ->
                    unstash id
                    merge_bi_command += " ${id}.json"
                }
                sh merge_bi_command

                // Publish build info
                String publish_command = "python publish_buildinfo.py --remote=${artifactory_credentials} ${buildInfoFilename}"
                sh publish_command
            }
        }
        */

        stage("Launch job-graph") {
            docker.image("conanio/gcc8").inside("--net=docker_jenkins_artifactory") {
                def scmVars = checkout scm
                //stage("Get project") {
                //    checkout scm
                //}

                stage("Configure Conan client") {
                    sh "conan config install ${config_url}".toString()
                }

                stage("Trigger dependents jobs") {
                    // Get the information about the package (ref + rrev)
                    name = sh (script: "conan inspect . --raw name", returnStdout: true).trim()
                    version = sh (script: "conan inspect . --raw version", returnStdout: true).trim()
                    sh "conan export . ${user_channel}"
                    
                    def search_output = "search_output.json"
                    sh "conan search ${name}/${version}@${user_channel} --revisions --raw --json=${search_output}"
                    def props = readJSON file: search_output
                    def revision = props[0]['revision']
                    def reference = "${name}/${version}@${user_channel}#${revision}"
                    echo "Full reference: '${reference}'"

                    // Trigger dependents jobs
                    
                    def repository = scmVars.GIT_URL.tokenize('/')[3].split("\\.")[0]
                    def sha1 = scmVars.GIT_COMMIT

                    def projects = ["ui-board-imgui/0.0@${user_channel}", ]  // TODO: Get list dinamically
                    projects.each {project_id -> 
                        def json = """{"parameter": [{"name": "reference", "value": "${reference}"}, \
                                                {"name": "project_id", "value": "${project_id}"}, \
                                                {"name": "organization", "value": "${organization}"}, \
                                                {"name": "repository", "value": "${repository}"}, \
                                                {"name": "sha1", "value": "${sha1}"} \
                                                ]}"""
                        withCredentials([usernamePassword(credentialsId: 'job-graph', passwordVariable: 'pass', usernameVariable: 'user')]) {
                            // TODO: FIXME: user pass from credentials
                            def url = "http://admin:1180edb4037ce3fb2dae7260d2cf4ddcb2@jenkins:8080/job/test_project/build"
                            sh "curl -v POST ${url} --data-urlencode json='${json}'"
                        }                            
                    }
                }
            }
        }
    }
    finally {
        deleteDir()
    }
}
