package check

import (
	"encoding/json"
	"fmt"
	"io/ioutil"
	"log"
	"net/http"
	"net/url"
	"time"

	"pep.cs.ru.nl/pep-watchdog/shared"
)

type pipelineAPIResponse struct {
	Status string `json:"status"`
	ID     int    `json:"id"`
}

func Pipelines(c Context) {
	if len(shared.Conf.CheckPipelines) > 0 &&
		shared.TokenFile.GitlabAPIToken == "" {

		log.Fatalf("Error: %v: 'gitlabAPIToken' not set",
			shared.Conf.TokenFile)
	}
	for _, pipelineConf := range shared.Conf.CheckPipelines {
		for _, branch := range pipelineConf.Branches {
			PipelineOf(c, pipelineConf.Project, branch)
		}
	}
}

func PipelineOf(c Context, project string, branch string) {
	httpClient := http.Client{
		Timeout: time.Duration(30 * time.Second),
	}

	urlFormat := fmt.Sprintf("https://gitlab.pep.cs.ru.nl/api/v4/projects/%s/pipelines?ref=%s&order_by=updated_at", url.PathEscape(project), url.QueryEscape(branch))
	apiResponse, err := getPage(c, httpClient, urlFormat, 1)
	for i := 2; len(apiResponse) > 0; i++ {
		//if err != nil, then apiResponse == nil, so we don't need to check err in the loop
		for _, pipeline := range apiResponse {
			switch pipeline.Status {
			case "success":
				return
			case "manual":
				return // We often don't run the manual deploy steps for e.g. prod. So if the pipeline got to that point, we consider it successful
			case "running": //We are not interested in jobs that are still running
			case "pending": //or have yet to start
			case "canceled": //or have been canceled before coming to either a successful or failed state
			case "skipped": //or are skipped (e.g. by having [ci_skip] in the commit message)
			default:
				c.Issuef("Last updated pipeline (#%d) for branch %s in project %s was not successful. Status is: %s", pipeline.ID, branch, project, pipeline.Status)
				return
			}
		}
		apiResponse, err = getPage(c, httpClient, urlFormat, i)
	}
	if err != nil {
		c.Issuef("%s", err)
		return
	}
	c.Issuef("No finished pipeline (either failed or succesful) found for branch %s in project %s", branch, project)
}

func getPage(c Context, httpClient http.Client, urlFormat string, page int) (apiResponse []pipelineAPIResponse, err error) {
	url := fmt.Sprintf("%s&page=%d", urlFormat, page)
	request, err := http.NewRequestWithContext(c, "GET", url, nil)
	if err != nil {
		err = fmt.Errorf("%s: error %s", url, err)
		return
	}

	request.Header.Add("PRIVATE-TOKEN",
		shared.TokenFile.GitlabAPIToken)

	response, err := httpClient.Do(request)
	if err != nil {
		err = fmt.Errorf("%s: error %s", url, err)
		return
	}
	defer response.Body.Close()

	if response.StatusCode != 200 {
		err = fmt.Errorf("%s: error. statuscode: %d", url, response.StatusCode)
		return
	}

	responseData, err := ioutil.ReadAll(response.Body)
	if err != nil {
		err = fmt.Errorf("%s: error reading response: %s", url, err)
		return
	}

	err = json.Unmarshal(responseData, &apiResponse)
	if err != nil {
		err = fmt.Errorf("%s: error parsing response: %s", url, err)
		return
	}

	return
}
